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
#include <ctype.h>

#include "sermux.h"

int	debug;
int	timeout;

void	usage();

/*
 * The daemon commences here...
 */
int
main(int argc, char *argv[])
{
	int i, portno;
	char *device;

	opterr = 0;
	portno = 5001;
	debug = 0;
	timeout = 5;
	while ((i = getopt(argc, argv, "dp:t:")) != EOF) {
		switch (i) {
		case 'p':
			if ((portno = atoi(optarg)) < 1 || portno > 65535)
				usage();
			break;

		case 't':
			if ((timeout = atoi(optarg)) < 1 || timeout > 3600)
				usage();
			break;

		case 'd':
			debug = 1;
			break;

		default:
			usage();
			break;
		}
	}
	if ((argc - optind) != 1)
		usage();
	device = strdup(argv[optind]);
	if (*device == '/')
		serial_master(device);
	else
		tcp_master(device);
	tcp_init(portno);
	while (1)
		chan_poll();
	exit(0);
}

/*
 * Crack open a string into its constituent components.
 */
int
crack(char *strp, char *argv[], int maxargs)
{
	int n;
	char *cp;

	while (isspace(*strp))
		strp++;
	for (n = 0; n < maxargs; n++) {
		argv[n] = strp;
		if ((cp = strchr(strp, ':')) != NULL)
			*cp++ = '\0';
		strp = cp;
		if (strp == NULL || *strp == '\0')
			break;
	}
	return(n + 1);
}

/*
 *
 */
void
error(char *msg)
{
	fprintf(stderr, "?Error - %s.\n", msg);
	exit(1);
}

/*
 *
 */
void
usage()
{
	fprintf(stderr, "Usage [-d][-p PORT][-t TIMEOUT] <device>\n");
	fprintf(stderr, "Where <device> is /dev/ttyUSB0:9600:8N1 or localhost:5000\n");
	exit(2);
}
