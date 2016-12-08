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
#include <string.h>
#include <sys/select.h>

#include "sermux.h"

struct	channel	*cfreelist;
struct	channel	*active;

int		maxfds;
fd_set		mrdfdset;
fd_set		mwrfdset;

/*
 *
 */
struct channel *
chan_alloc()
{
	static int initted = 0, last_channo;
	struct channel *chp;

	if (!initted) {
		cfreelist = active = NULL;
		maxfds = last_channo = 0;
		FD_ZERO(&mrdfdset);
		FD_ZERO(&mwrfdset);
		initted = 1;
	}
	if ((chp = cfreelist) != NULL)
		cfreelist = chp->next;
	else {
		if ((chp = (struct channel *)malloc(sizeof(struct channel))) == NULL) {
			perror("channel_alloc: malloc");
			error("cannot allocate new channel");
		}
	}
	chp->fd = -1;
	chp->channo = ++last_channo;
	chp->next = chp->mqnext = NULL;
	chp->bqhead = NULL;
	chp->totread = 0;
	chp->read_proc = NULL;
	chp->write_proc = NULL;
	if (active == NULL)
		active = chp;
	else {
		struct channel *nchp;

		for (nchp = active; nchp->next != NULL; nchp = nchp->next)
			;
		nchp->next = chp;
	}
	printf("New channel %d.\n", chp->channo);
	return(chp);
}

/*
 *
 */
void
chan_free(struct channel *chp)
{
	if (chp->fd >= 0)
		close(chp->fd);
	chp->fd = -1;
	if (active == chp)
		active = chp->next;
	else {
		struct channel *nchp;

		for (nchp = active; nchp->next != NULL; nchp = nchp->next) {
			if (nchp->next == chp) {
				nchp->next = chp->next;
				break;
			}
		}
	}
	chp->next = cfreelist;
	cfreelist = chp;
}

/*
 *
 */
void
chan_poll()
{
	int n, bad;
	struct channel *chp, *nchp;
	struct timeval tval, *tvp;
	fd_set rfds, wfds;

	memcpy((char *)&rfds, (char *)&mrdfdset, sizeof(fd_set));
	memcpy((char *)&wfds, (char *)&mwrfdset, sizeof(fd_set));
	printf("Selecting on %d FDs...\n", maxfds);
	if (proc_busy()) {
		tvp = &tval;
		tvp->tv_sec = timeout;
		tvp->tv_usec = 0;
		printf("Timeout of %d seconds\n", timeout);
	} else
		tvp = NULL;
	if ((n = select(maxfds, &rfds, &wfds, NULL, tvp)) < 0) {
		perror("select");
		error("chan_poll: select failed");
	}
	printf("Select returned %d.\n", n);
	if (n == 0) {
		proc_timeout();
		return;
	}
	/*
	 * We have some sort of r/w data or an event. Look through the
	 * channels to see what's new.
	 */
	for (chp = active; chp != NULL; chp = chp->next) {
		if (chp->fd >= 0) {
			bad = 0;
			if (FD_ISSET(chp->fd, &rfds))
				if (chp->read_proc == NULL || chp->read_proc(chp) < 0)
					bad = 1;
			if (!bad && FD_ISSET(chp->fd, &wfds))
				if (chp->write_proc == NULL || chp->write_proc(chp) < 0)
					bad = 1;
			if (bad) {
				/*
				 * Channel has failed. Clean this up during
				 * the switch phase, but mark it closed.
				 */
				printf("Bad:%d\n", bad);
				chan_readoff(chp);
				chan_writeoff(chp);
				close(chp->fd);
				chp->fd = -1;
			}
		}
	}
	/*
	 * Hunt for inactive channels and remove them.
	 * Start with the inactive channels at the head of the queue.
	 */
	while (active->fd < 0) {
		active = chp->next;
		proc_release(chp);
		chan_free(chp);
	}
	if (active == NULL)
		return;
	/*
	 * Now look for ones in the middle of the queue.
	 */
	for (chp = active; chp->next != NULL; chp = nchp) {
		nchp = chp->next;
		if (nchp->fd < 0) {
			chp->next = nchp->next;
			chp = nchp;
			nchp = chp->next;
			proc_release(chp);
			chan_free(chp);
		}
	}
}

/*
 *
 */
void
chan_readon(struct channel *chp)
{
	printf("Read ON on channel%d\n", chp->channo);
	if (chp == NULL || chp->fd < 0)
		return;
	if (maxfds <= chp->fd)
		maxfds = chp->fd + 1;
	FD_SET(chp->fd, &mrdfdset);
}

/*
 *
 */
void
chan_readoff(struct channel *chp)
{
	printf("Read OFF on channel%d\n", chp->channo);
	if (chp != NULL && chp->fd >= 0)
		FD_CLR(chp->fd, &mrdfdset);
}

/*
 *
 */
void
chan_writeon(struct channel *chp)
{
	printf("Write ON on channel%d\n", chp->channo);
	if (chp == NULL || chp->fd < 0)
		return;
	if (maxfds <= chp->fd)
		maxfds = chp->fd + 1;
	FD_SET(chp->fd, &mwrfdset);
}

/*
 *
 */
void
chan_writeoff(struct channel *chp)
{
	printf("Write OFF on channel%d\n", chp->channo);
	if (chp != NULL && chp->fd >= 0)
		FD_CLR(chp->fd, &mwrfdset);
}
