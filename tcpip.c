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
#include <syslog.h>
#include <string.h>

#include "sermux.h"

int		accept_fd;

/*
 *
 */
int
tcp_newconn()
{
	socklen_t len;
	struct channel *chp;
	struct sockaddr_in sin;

	printf("New connection received.\n");
	len = sizeof(struct sockaddr_in);
	chp = chan_alloc();
	printf("Going to accept...\n");
	if ((chp->fd = accept(accept_fd, (struct sockaddr *)&sin, &len)) < 0) {
		syslog(LOG_INFO, "tcp_newconn failed: %m");
		return(-1);
	}
	printf("Back from accept(). Addr:[%08x]\n", sin.sin_addr.s_addr);
	chan_readon(chp->fd);
	return(1);
}

/*
 *
 */
void
tcp_init(int port)
{
	int yes = 1;
	struct sockaddr_in sin;

	printf("TCP INIT: Port%d\n", port);
	if ((accept_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("tcp_init: socket failed");
		exit(1);
	}
	if (setsockopt(accept_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
		perror("tcp_init: setsockopt failed");
		exit(1);
	}
	memset((char *)&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);
	if (bind(accept_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("tcp_init: bind");
		exit(1);
	}
	listen(accept_fd, 64);
	printf("LISTEN Socket UP!\n");
	chan_readon(accept_fd);
}

/*
 *
 */
void
tcp_master(char *argstr)
{
	int i, port;
	char *params[4], *host;
	struct sockaddr_in sin;
	in_addr_t addr;

	printf("TCP Remote Init: [%s]\n", argstr);
	if ((i = crack(argstr, params, 4)) < 1)
		usage();
	host = params[0];
	port = (i > 1) ? atoi(params[1]) : 5000;
	if (host == NULL || *host == '\0')
		usage();
	if ((addr = inet_addr(host)) == INADDR_NONE) {
		struct hostent *hp;

		if ((hp = gethostbyname(host)) == NULL) {
			fprintf(stderr, "?Unresolved hostname: %s\n", host);
			exit(1);
		}
		memcpy((char *)&addr, hp->h_addr, hp->h_length);
	}
	printf("%08x\n", addr);
	master = chan_alloc();
	if ((master->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("tcp_master: socket");
		exit(1);
	}
	memset((char *)&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	sin.sin_port = htons(port);
	if (connect(master->fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("tcp_master: connect failed");
		exit(1);
	}
	chan_readon(master->fd);
}
