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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

#include "sermux.h"

/*
 *
 */
int
tcp_newconn(struct channel *chp)
{
	socklen_t len;
	struct channel *nchp;
	struct sockaddr_in sin;

	printf("New connection received.\n");
	len = sizeof(struct sockaddr_in);
	nchp = chan_alloc();
	printf("Going to accept...\n");
	if ((nchp->fd = accept(chp->fd, (struct sockaddr *)&sin, &len)) < 0) {
		perror("tcp_newconn");
		return(-1);
	}
	printf("Back from accept(). Addr:[%08x]\n", sin.sin_addr.s_addr);
	nchp->read_proc = slave_read;
	nchp->write_proc = slave_write;
	chan_readon(nchp);
	return(1);
}

/*
 *
 */
void
tcp_init(int port)
{
	int yes = 1;
	struct channel *chp = chan_alloc();
	struct sockaddr_in sin;

	printf("TCP INIT: Port%d\n", port);
	if ((chp->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		error("tcp_init: socket");
	if (setsockopt(chp->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0)
		error("tcp_init: setsockopt");
	memset((char *)&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);
	if (bind(chp->fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("tcp_init: bind");
		error("cannot bind to socket");
	}
	listen(chp->fd, 64);
	printf("LISTEN Socket UP!\n");
	chp->read_proc = tcp_newconn;
	chan_readon(chp);
}

/*
 *
 */
void
tcp_master(char *argstr)
{
	int i, port;
	char *params[4], *host;
	struct channel *chp = chan_alloc();
	struct sockaddr_in sin;
	in_addr_t addr;

	printf("TCP Remote Init: [%s]\n", argstr);
	if ((i = crack(argstr, params, 4)) < 1)
		error("invalid remote tcp parameters");
	host = params[0];
	port = (i > 1) ? atoi(params[1]) : 5000;
	if (host == NULL || *host == '\0')
		error("invalid remote host specification");
	if ((addr = inet_addr(host)) == INADDR_NONE) {
		struct hostent *hp;

		if ((hp = gethostbyname(host)) == NULL) {
			fprintf(stderr, "Unresolved hostname: %s\n", host);
			error("cannot find remote host");
		}
		memcpy((char *)&addr, hp->h_addr, hp->h_length);
	}
	printf("%08x\n", addr);
	if ((chp->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		error("tcp_master: socket");
	memset((char *)&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	sin.sin_port = htons(port);
	if (connect(chp->fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("tcp_master: connect");
		error("cannot connect to remote host");
	}
	proc_set_master(chp);
}
