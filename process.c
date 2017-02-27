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
#include <syslog.h>
#include <string.h>

#include "sermux.h"

struct	channel		*master = NULL;

/*
 * We have read data on the master socket. Deal with it.
 */
int
master_read()
{
	printf("MASTER Read!\n");
	/*
	 * Do we have any slaves? If not, dump what we've just read.
	 */
	buf_read(master);
	if (busyq.head == NULL) {
		buf_flush(master);
		return(0);
	}
	/*
	 * If we've a lot of data backed up, stop reading (for now).
	 */
	if (master->totread > (READSIZE * 10))
		chan_readoff(master->fd);
	else
		chan_readon(master->fd);
	printf("Master has %d bytes buffered.\n", master->totread);
	/*
	 * OK, queue up the slave for writing...
	 */
	chan_writeon(busyq.head->fd);
	return(1);
}

/*
 * We can write to the master socket.
 */
int
master_write()
{
	int n;

	printf("MASTER Write!\n");
	if (busyq.head == NULL) {
		/*
		 * Oops! No slaves. Nothing to see, here...
		 */
		chan_writeoff(master->fd);
		return(0);
	}
	/*
	 * Write data from the current head of the busy Q to the
	 * master socket. Disable writes if there's no more data.
	 */
	printf("Slave channel %d wants to write.\n", busyq.head->channo);
	n = buf_write(busyq.head, master->fd);
	if (busyq.head->bhead == NULL)
		chan_writeoff(master->fd);
	return(n);
}

/*
 * Read data from a slave. Make sure this guy is bumped up to the busy Q.
 */
int
slave_read(struct channel *chp)
{
	int n;

	printf("SLAVE Read on channel %d\n", chp->channo);
	/*
	 * If we've a lot of data backed up, stop reading (for now).
	 */
	n = buf_read(chp);
	if (chp->totread > (READSIZE * 10))
		chan_readoff(chp->fd);
	else
		chan_readon(chp->fd);
	/*
	 * Move him to the busy Q and mark the time of this read.
	 */
	qmove(chp, BUSY_Q);
	chp->last_read = last_event;
	return(n);
}

/*
 * Time to write master data to the slave. This is pretty
 * straightforward. Just do a write of the master data, and disable any
 * future slave writes until we have more master data.
 */
int
slave_write(struct channel *chp)
{
	printf("SLAVE Write on channel %d\n", chp->channo);
	if (master->bhead == NULL || chp != busyq.head) {
		/*
		 * Oops! Nothing to write. Dunno how we got here, but
		 * leave gracefully. Also, only allow writes to the head
		 * of the busy Q.
		 */
		chan_writeoff(chp->fd);
		return(0);
	}
	buf_write(master, chp->fd);
	if (master->bhead == NULL)
		chan_writeoff(chp->fd);
	return(0);
}

/*
 * Guy at the head of the Q has had his chance. Time to bump him to the idle
 * Q and let the next guy have a chance. Presumably the new guy at the head
 * of the queue has buffer data. If so, enable master writes. Also, to avoid
 * spurious timeouts, reset his last_read time.
 */
void
slave_promote()
{
	if (busyq.head->next == NULL)
		return;
	qmove(busyq.head, IDLE_Q);
	if (busyq.head->bhead != NULL) {
		chan_writeon(master->fd);
		busyq.head->last_read = last_event;
	}
}
