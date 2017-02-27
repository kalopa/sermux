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
#define LINELEN		512
#define MAXPORTS	16

#define FREE_Q		0
#define IDLE_Q		1
#define BUSY_Q		2

struct	channel {
	struct	channel	*next;
	struct	buffer	*bqhead;
	int				channo;
	int				qid;
	int				fd;
	int				totread;
	time_t			last_read;
};

struct	buffer	{
	struct	buffer	*next;
	int				size;
	char			data[LINELEN];
};

struct 	queue	{
	struct 	channel	*head;
	struct 	channel	*tail;
};

extern int				accept_fd;
extern int				debug;
extern int				timeout;
extern struct channel	*master;
extern struct queue		freeq;
extern struct queue		idleq;
extern struct queue		busyq;
extern time_t			last_event;

void		tcp_init(int);
void		tcp_master(char *);
int			tcp_newconn();
void		serial_master(char *);
int			master_read();
int			master_write();
int			slave_read(struct channel *);
int			slave_write(struct channel *);
void		queue_init();
void		enqueue(struct queue *, struct channel *);
void		dequeue(struct queue *, struct channel *);
void		qmove(struct channel *, int);
int			contention();
void		chan_init();
void		chan_process(struct queue *);
void		chan_poll();
struct	channel	*chan_alloc();
void		chan_readon(int);
void		chan_readoff(int);
void		chan_writeon(int);
void		chan_writeoff(int);
struct	buffer	*buf_alloc();
void		buf_free(struct buffer *);
void		buf_flush(struct channel *);
int			buf_read(struct channel *);
int			buf_write(struct channel *, int);
int			crack(char *, char *[], int);
void		usage();
void		hexdump(char *, int, int);
