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
 *
 */
void
proc_set_master(struct channel *chp)
{
	master = chp;
	printf("MASTER is Channel%d\n", chp->channo);
	chp->read_proc = master_read;
	chp->write_proc = master_write;
	chan_readon(chp);
}

/*
 *
 */
void
proc_set_slave(struct channel *chp)
{
	printf("Setting channel %d as new active slave.\n", chp->channo);
	{
		struct channel *xp;

		for (xp = master; xp != NULL; xp = xp->mqnext)
			printf("CH%d...\n", xp->channo);
	}
	if (master->mqnext == NULL) {
		/*
		 * Master is free and we have a new slave.
		 */
		master->mqnext = chp;
	} else {
		struct channel *nchp;

		/*
		 * Oops! We already have a running slave - go to the
		 * back of the queue. You'll have to wait for this
		 * current guy to go idle...
		 */
		for (nchp = master; nchp->mqnext != NULL && nchp->mqnext != chp; nchp = nchp->mqnext)
			;
		nchp->mqnext = chp;
	}
	chan_readon(master);
	chan_writeon(master);
}

/*
 *
 */
void
proc_release(struct channel *chp)
{
	struct channel *nchp;

	if (master == NULL || master->mqnext == NULL)
		return;
	if (master == chp) {
		syslog(LOG_ERR, "attempt to release master channel");
		exit(1);
	}
	if (master->mqnext == chp) {
		/*
		 * This current slave channel is dead. Oh well...
		 */
		master->mqnext = chp->mqnext;
	} else {
		/*
		 * Check if the slave is in our master queue. If so,
		 * remove it.
		 */
		for (nchp = master; nchp->mqnext != NULL; nchp = nchp->mqnext) {
			if (nchp->next == chp) {
				nchp->next = chp->next;
				break;
			}
		}
	}
	chp->mqnext = NULL;
}

/*
 * Do we need a timeout in the select-loop? Yes, if we're doing work
 * for a slave.
 */
int
proc_busy()
{
	return((master != NULL && master->mqnext != NULL && master->mqnext->mqnext != NULL) ? 1 : 0);
}

/*
 * We timed-out waiting for data. Time to kick the current slave off
 * the head of the queue.
 */
void
proc_timeout()
{
	struct channel *chp;

	printf("TIMEOUT!\n");
	if (proc_busy()) {
		chp = master->mqnext;
		master->mqnext = chp->mqnext;
		chp->mqnext = NULL;
		chan_readon(master);
		chan_writeon(master);
	}
}

/*
 *
 */
int
master_read(struct channel *chp)
{
	printf("MASTER Read!\n");
	buf_read(chp);
	/*
	 * Do we have any slaves? If not, dump what we've just read.
	 */
	if ((chp = master->mqnext) == NULL) {
		buf_flush(master);
		chan_readoff(master);
		return(0);
	}
	/*
	 * If we've a lot of data backed up, stop reading (for now).
	 */
	if (chp->totread > (LINELEN * 10))
		chan_readoff(chp);
	else
		chan_readon(chp);
	printf("Master has %d bytes buffered.\n", chp->totread);
	/*
	 * OK, queue up the slave for writing...
	 */
	chan_writeon(chp);
	return(0);
}

/*
 *
 */
int
master_write(struct channel *chp)
{
	int n;

	printf("MASTER Write!\n");
	if ((chp = master->mqnext) == NULL || chp->bqhead == NULL) {
		/*
		 * Oops! No slaves. Nothing to see, here...
		 */
		chan_writeoff(master);
		return(0);
	}
	printf("Slave channel %d wants to write.\n", chp->channo);
	n = buf_write(chp, master->fd);
	if (chp->bqhead == NULL)
		chan_writeoff(master);
	return(n);
}

/*
 * Read data from a slave. If the master is currently processing an
 * account then we just enqueue this one behind the earlier request(s). If
 * it's the head of the queue, then set things up for a master-write.
 */
int
slave_read(struct channel *chp)
{
	int n;

	printf("SLAVE Read on channel %d\n", chp->channo);
	n = buf_read(chp);
	/*
	 * If we've a lot of data backed up, stop reading (for now).
	 */
	if (chp->totread > (LINELEN * 10))
		chan_readoff(chp);
	else
		chan_readon(chp);
	/*
	 * Error-out if no master (which can't happen).
	 */
	if (master == NULL) {
		syslog(LOG_ERR, "panic: no master!");
		exit(1);
	}
	proc_set_slave(chp);
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
	if (master == NULL || master->mqnext == NULL || master->mqnext != chp || master->bqhead == NULL) {
		/*
		 * Oops! Nothing to write. Dunno how we got here, but
		 * leave gracefully.
		 */
		chan_writeoff(chp);
		return(0);
	}
	buf_write(master, chp->fd);
	if (master->bqhead == NULL)
		chan_writeoff(chp);
	return(0);
}
