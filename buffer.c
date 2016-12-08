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

struct	buffer	*bfreelist = NULL;

/*
 *
 */
struct buffer *
buf_alloc()
{
	struct buffer *bp;

	if ((bp = bfreelist) != NULL)
		bfreelist = bp->next;
	else {
		if ((bp = (struct buffer *)malloc(sizeof(struct buffer))) == NULL) {
			syslog(LOG_ERR, "buf_alloc() malloc: %m");
			exit(1);
		}
	}
	bp->size = 0;
	bp->next = NULL;
	return(bp);
}

/*
 *
 */
void
buf_free(struct buffer *bp)
{
	bp->next = bfreelist;
	bfreelist = bp;
}

/*
 *
 */
void
buf_flush(struct channel *chp)
{
	struct buffer *bp;

	while ((bp = chp->bqhead) != NULL) {
		chp->bqhead = bp->next;
		buf_free(bp);
	}
}

/*
 *
 */
int
buf_read(struct channel *chp)
{
	struct buffer *bp = buf_alloc();

	/*
	 * Read from the device.
	 */
	if ((bp->size = read(chp->fd, bp->data, LINELEN)) <= 0) {
		buf_free(bp);
		return(-1);
	}
	printf("Read %d bytes.\n", bp->size);
	hexdump(bp->data, 0, bp->size);
	/*
	 * Stick the buffer on the end of the channel read queue.
	 */
	chp->totread += bp->size;
	if (chp->bqhead == NULL)
		chp->bqhead = bp;
	else {
		struct buffer *nbp;

		for (nbp = chp->bqhead; nbp->next != NULL; nbp = nbp->next)
			;
		nbp->next = bp;
	}
	return(bp->size);
}

/*
 *
 */
int
buf_write(struct channel *chp, int wrfd)
{
	int n;
	struct buffer *bp;

	/*
	 * Pull the buffer from the head of the queue and write it out
	 * to the specified device.
	 */
	bp = chp->bqhead;
	chp->bqhead = bp->next;
	if ((n = write(wrfd, bp->data, bp->size)) != bp->size) {
		syslog(LOG_INFO, "buffer write failure");
		return(-1);
	}
	printf("Wrote %d bytes.\n", n);
	/*
	 * Release the buffer and we're done.
	 */
	chp->totread -= n;
	buf_free(bp);
	return(n);
}
#if 0
/*
 * Do the buffer-switching activities.
 */
void
chan_switch()
{
	struct buffer *chp, *nchp;

	printf("Checking channels for read/write data...\n");
	for (chp = chead; chp != NULL; chp = nchp) {
		printf("Switch chan%d...\n", chp->channo);
		nchp = chp->next;
		if (chp->rdlen > 0) {
			printf("Channel %d has read data.\n", chp->channo);
			if (chp->switch_proc != NULL)
				chp->switch_proc(chp);
		}
		/*
		 * Check if the device is closed/borked.
		 */
		if (chp->fd < 0) {
			printf("Borking...\n");
			/*
			 * Yes - let it go.
			 */
			if (chp->prev == NULL)
				chead = nchp;
			else
				chp->prev->next = nchp;
			if (nchp == NULL)
				ctail = chp->prev;
			else
				nchp->prev = chp->prev;
			chan_free(chp);
		}
	}
	for (chp = chead; chp != NULL; chp = chp->next) {
		if (chp->wrlen > 0) {
			printf("Channel %d has write data.\n", chp->channo);
			chan_enable(chp->fd, CHANFD_RD|CHANFD_WR);
		} else
			chan_enable(chp->fd, CHANFD_RD);
	}
}

/*
 * The default buffer-copy switch. This doesn't do anything useful.
 */
int
chan_copyio(struct channel *chp)
{
	int len;

	if ((len = BUFFER_SIZE - chp->wrlen) > chp->rdlen)
		len = chp->rdlen;
	printf("chan_copyio on channel %d, copying %d bytes.\n", chp->channo, len);
	memcpy(chp->wrbuffer + chp->wrlen, chp->rdbuffer, len);
	chp->wrlen += len;
	chp->rdlen -= len;
	if (chp->rdlen > 0) {
		/*
		 * Argh! Need to collapse the buffer. Please don't
		 * do this...
		 */
		memmove(chp->rdbuffer, chp->rdbuffer + len, chp->rdlen);
	}
	return(len);
}
#endif
