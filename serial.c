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
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <syslog.h>
#include <string.h>

#include "sermux.h"

struct	speed	{
	int	value;
	int	code;
} speeds[] = {
	{50, B50},
	{75, B75},
	{110, B110},
	{134, B134},
	{150, B150},
	{200, B200},
	{300, B300},
	{600, B600},
	{1200, B1200},
	{1800, B1800},
	{2400, B2400},
	{4800, B4800},
	{9600, B9600},
	{19200, B19200},
	{38400, B38400},
	{57600, B57600},
	{115200, B115200},
	{230400, B230400},
	{0, 0}
};

/*
 *
 */
void
serial_master(char *argstr)
{
	int i, baud;
	char *settings, *params[4];
	struct channel *chp = chan_alloc();
	struct termios tios;

	printf("Serial Init: [%s]\n", argstr);
	if ((i = crack(argstr, params, 4)) < 1)
		usage();

	baud = (i > 1) ? atoi(params[1]) : 9600;
	settings = (i > 2) ? params[2] : "8N1";
	if (strlen(settings) != 3)
		usage();
	if ((chp->fd = open(params[0], O_RDWR|O_NOCTTY|O_NDELAY)) < 0) {
		fprintf(stderr, "?Cannot open serial device: ");
		perror(params[0]);
		exit(1);
	}
	/*
	 * Get and set the tty parameters.
	 */
	printf(">SERIAL FD%d:, Spd: %d, Params: [%s]\n", chp->fd, baud, settings);
	if (tcgetattr(chp->fd, &tios) < 0) {
		perror("serial_master: tcgetattr failed");
		exit(1);
	}
	for (i = 0; speeds[i].value > 0; i++)
		if (speeds[i].value == baud)
			break;
	if (speeds[i].value == 0) {
		fprintf(stderr, "?Invalid serial baud rate: %d\n", baud);
		exit(1);
	}

	cfsetispeed(&tios, speeds[i].code);
	cfsetospeed(&tios, speeds[i].code);
	tios.c_cflag &= ~(CSIZE|PARENB|CSTOPB);
	tios.c_cflag |= (CLOCAL|CREAD);
	switch (settings[0]) {
	case 5:
		tios.c_cflag |= CS5;
		break;

	case 6:
		tios.c_cflag |= CS6;
		break;

	case 7:
		tios.c_cflag |= CS7;
		break;

	default:
		tios.c_cflag |= CS8;
		break;
	}
	switch (settings[1]) {
	case 'E':
	case 'e':
		tios.c_cflag |= PARENB;
		break;

	case 'O':
	case 'o':
		tios.c_cflag |= (PARENB|PARODD);
		break;
	}
	if (settings[2] == '2')
		tios.c_cflag |= CSTOPB;
	tios.c_lflag = tios.c_iflag = tios.c_oflag = 0;
	tios.c_cc[VMIN] = 0;
	tios.c_cc[VTIME] = 10;
	if (tcsetattr(chp->fd, TCSANOW, &tios) < 0) {
		perror("serial_master: tcsetattr failed");
		exit(1);
	}
	proc_set_master(chp);
}
