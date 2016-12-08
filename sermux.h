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

struct	channel {
	struct	channel	*next;
	struct	channel	*mqnext;
	struct	buffer	*bqhead;
	int		channo;
	int		fd;
	int		totread;
	int		(*read_proc)(struct channel *);
	int		(*write_proc)(struct channel *);
};

struct	buffer	{
	struct	buffer	*next;
	int		size;
	char		data[LINELEN];
};

extern	int	debug;
extern	int	timeout;

void		proc_set_master(struct channel *);
void		proc_set_slave(struct channel *);
int		proc_busy();
void		proc_timeout();
void		proc_release(struct channel *);
int		master_read(struct channel *);
int		master_write(struct channel *);
int		slave_read(struct channel *);
int		slave_write(struct channel *);
void		tcp_init(int);
void		serial_master(char *);
void		tcp_master(char *);
void		chan_poll();
struct	channel	*chan_alloc();
void		chan_free(struct channel *);
void		chan_readon(struct channel *);
void		chan_readoff(struct channel *);
void		chan_writeon(struct channel *);
void		chan_writeoff(struct channel *);
struct	buffer	*buf_alloc();
void		buf_free(struct buffer *);
void		buf_flush(struct channel *);
int		buf_read(struct channel *);
int		buf_write(struct channel *, int);
int		crack(char *, char *[], int);
void		error(char *);
void		hexdump(char *, int, int);
