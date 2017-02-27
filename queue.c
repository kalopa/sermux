/*
 * Copyright (c) 2016, Kalopa Research.  All rights reserved.  This is free
 * software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation;
 * either version 2, or (at your option) any later version.
 *
 * It is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this product; see the file COPYING.  If not, write to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * THIS SOFTWARE IS PROVIDED BY KALOPA RESEARCH "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL KALOPA RESEARCH BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <syslog.h>
#include <string.h>

#include "sermux.h"

struct queue		freeq;
struct queue		idleq;
struct queue		busyq;

struct queue		*queues[3];

/*
 * Initialize the queueing subsystem.
 */
void
queue_init()
{
	freeq.head = freeq.tail = NULL;
	idleq.head = idleq.tail = NULL;
	busyq.head = busyq.tail = NULL;
	queues[FREE_Q] = &freeq;
	queues[IDLE_Q] = &idleq;
	queues[BUSY_Q] = &busyq;
}

/*
 * Enqueue a channel on the specified queue.
 */
void
enqueue(struct queue *qp, struct channel *chp)
{
	chp->next = NULL;
	if (qp->head == NULL)
		qp->head = chp;
	else
		qp->tail->next = chp;
	qp->tail = chp;
}

/*
 * Remove a channel from the specified queue.
 */
void
dequeue(struct queue *qp, struct channel *chp)
{
	if (qp->head == chp) {
		/*
		 * On the head of the queue. Do this the easy way.
		 */
		if ((qp->head = chp->next) == NULL) {
			qp->tail = NULL;
			return;
		}
	} else {
		struct channel *nchp;

		for (nchp = qp->head; nchp->next != NULL; nchp = nchp->next) {
			if (nchp->next == chp) {
				nchp->next = chp->next;
				break;
			}
		}
	}
	/*
	 * Now make sure the tail pointer is valid.
	 */
	for (qp->tail = qp->head; qp->tail->next != NULL; qp->tail = qp->tail->next)
		;
}

/*
 * Move the channel from its current queue to another queue.
 */
void
qmove(struct channel *chp, int newqid)
{
	if (chp->qid == newqid)
		return;
	dequeue(queues[chp->qid], chp);
	chp->qid = newqid;
	enqueue(queues[newqid], chp);
}

/*
 * Return TRUE if more than one channel wants access.
 */
int
contention()
{
	return(busyq.head != NULL && busyq.head->next != NULL);
}
