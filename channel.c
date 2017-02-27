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
#include <time.h>
#include <sys/select.h>
#include <syslog.h>
#include <string.h>

#include "sermux.h"

int			maxfds;
int 		last_channo;
fd_set		mrdfdset, rfds;
fd_set		mwrfdset, wfds;
time_t		last_event;

/*
 * Initialize the channel structures, including the array of file descriptors
 * used by select(2).
 */
void
chan_init()
{
	maxfds = last_channo = 0;
	FD_ZERO(&mrdfdset);
	FD_ZERO(&mwrfdset);
}

/*
 * Allocate a channel. If there's one on the free Q then take that. Otherwise,
 * allocate one from malloc(3).
 */
struct channel *
chan_alloc()
{
	struct channel *chp;

	if ((chp = freeq.head) != NULL)
		dequeue(&freeq, chp);
	else {
		if ((chp = (struct channel *)malloc(sizeof(struct channel))) == NULL) {
			syslog(LOG_ERR, "chan_alloc() malloc: %m");
			exit(1);
		}
	}
	/*
	 * Initialize the channel. Note that all new channels default to the idle Q
	 */
	chp->fd = -1;
	chp->channo = ++last_channo;
	chp->qid = IDLE_Q;
	enqueue(&idleq, chp);
	chp->bhead = NULL;
	chp->totread = 0;
	chp->last_read = 0;
	chp->next = NULL;
	printf("New channel %d.\n", chp->channo);
	return(chp);
}

/*
 * Process any read/write activity on the given channel queue.
 */
void
chan_process(struct queue *qp)
{
	int fail;
	struct channel *chp, *nchp;

	for (chp = qp->head; chp != NULL; chp = nchp) {
		nchp = chp->next;
		if (chp->fd < 0)
			continue;
		if (chp == master) {
			/*
			 * Check for master read/write.
			 */
			if (FD_ISSET(master->fd, &rfds))
				master_read();
			if (FD_ISSET(master->fd, &wfds))
				master_write();
			continue;
		}
		fail = 0;
		if (FD_ISSET(chp->fd, &rfds) && slave_read(chp) < 0)
			fail = 1;
		if (!fail && FD_ISSET(chp->fd, &wfds) && slave_write(chp) < 0)
			fail = 1;
		if (fail) {
			/*
			 * Channel has failed. Clean this up and move it to the
			 * free Q.
			 */
			printf("Channel%d failure: %d\n", chp->channo, fail);
			chan_readoff(chp->fd);
			chan_writeoff(chp->fd);
			close(chp->fd);
			chp->fd = -1;
			qmove(chp, FREE_Q);
		}
	}
}

/*
 * Poll (select(2)) the list of channels (and the I/O device) for read/write
 * activity and handle accordingly.
 */
void
chan_poll()
{
	int n;
	struct timeval tval, *tvp = NULL;

	/*
	 * Maintain a copy of the file descriptor sets, because it's too hard
	 * to initialize the entire array each time.
	 */
	memcpy((char *)&rfds, (char *)&mrdfdset, sizeof(fd_set));
	memcpy((char *)&wfds, (char *)&mwrfdset, sizeof(fd_set));
	printf(">>>> TOP OF LOOP: Select on %d FDs...\n", maxfds);
	if (contention()) {
		/*
		 * We have contention - at least two channels want the device.
		 * We use a timeout so that we'll give up on the current head
		 * of the busy queue if they don't do anything with it.
		 */
		tvp = &tval;
		tvp->tv_sec = timeout;
		tvp->tv_usec = 0;
		printf("Timeout=%ds\n", timeout);
	} else {
		tvp = NULL;
		printf("No timeout\n");
	}
	if ((n = select(maxfds, &rfds, &wfds, NULL, tvp)) < 0) {
		syslog(LOG_ERR, "select failure in chan_poll: %m");
		exit(1);
	}
	/*
	 * Get the time-of-day, because this will be used for timestamping
	 * various events, and for computing actual timeouts. Check to see,
	 * for example, if the head of the queue is idle. If so, promote
	 * the next guy (assuming there is one).
	 */
	time(&last_event);
	printf("Select returned %d.\n", n);
	if (contention() && (last_event - busyq.head->last_read) >= timeout)
		slave_promote();
	if (n == 0)
		return;
	/*
	 * Check for a new connection on the listen(2) socket. Then check for
	 * read/write data (or events) on the various queues.
	 */
	if (FD_ISSET(accept_fd, &rfds))
		tcp_newconn();
	chan_process(&busyq);
	chan_process(&idleq);
}

/*
 * Enable read polling on the specified file descriptor.
 */
void
chan_readon(int fd)
{
	printf("Read ON on fd%d\n", fd);
	if (fd < 0)
		return;
	if (maxfds <= fd)
		maxfds = fd + 1;
	FD_SET(fd, &mrdfdset);
}

/*
 * Disable read polling on the specified file descriptor.
 */
void
chan_readoff(int fd)
{
	printf("Read OFF on fd%d\n", fd);
	if (fd >= 0)
		FD_CLR(fd, &mrdfdset);
}

/*
 * Enable write polling on the specified file descriptor.
 */
void
chan_writeon(int fd)
{
	printf("Write ON on fd%d\n", fd);
	if (fd < 0)
		return;
	if (maxfds <= fd)
		maxfds = fd + 1;
	FD_SET(fd, &mwrfdset);
}

/*
 * Disable write polling on the specified file descriptor.
 */
void
chan_writeoff(int fd)
{
	printf("Write OFF on fd%d\n", fd);
	if (fd >= 0)
		FD_CLR(fd, &mwrfdset);
}
