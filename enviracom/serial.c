/*
 * @(#)$Id: serial.c,v 1.3 2008/11/04 17:28:15 srp Exp srp $
 *
 * Copyright (C) 1998 - 2000 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the POSIX termios serial interface to THERM.
 * termio to termios conversion by Pat Jensen <patj@futureunix.net>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <string.h>

#include "enviracom.h"

int speed=B19200, bits=CS8, stopbits=1, parity=0;

char *device = NULL;		/* serial device name */
struct termios stbuf, svbuf;	/* termios: svbuf=saved, stbuf=set */

/* serial cleanup routine called as the program exits */
void
cleanup_serial(int fd)
{
    if (fd > 0) {
	/* Restore the original settings */
	if (tcsetattr(fd, TCSANOW, &svbuf) < 0) {
	    sprintf(errbuf, "%s: can't tcsetattr device %s", progname, device);
	    perror(errbuf);
	}
	close(fd);
    }
}
	
/* return file descriptor of successfully opened serial device or negative # */
int
init_serial(char *dev)
{
    int fd = -1;		/* serial device file descriptor */
    int flags;

    if (dev != NULL)
	device = dev;
    else if ((device = getenv("ENVIRACOM")) == NULL) /* default to env var */
	device = ENVIRACOM;	/* or built-in default */

#ifdef O_NONBLOCK
    /* Open with non-blocking I/O, we'll fix after we set CLOCAL */
    if ((fd = open(device, O_RDWR|O_NONBLOCK)) < 0) {
#else
	if ((fd = open(device, O_RDWR)) < 0) {
#endif
	    sprintf(errbuf, "%s: can't open device %s", progname, device);
	    perror(errbuf);
	    return(-1);
	}

    if (tcgetattr(fd, &svbuf) < 0) {
	sprintf(errbuf, "%s: can't tcgetattr device %s", progname, device);
	perror(errbuf);
	close(fd);
	return(-2);
    }

    /* Save the original settings */
    (void) memcpy(&stbuf, &svbuf, sizeof(struct termios));

    /* Set the speeds */
    cfsetospeed(&stbuf,speed);
    cfsetispeed(&stbuf,speed);

    /* Set to 8N1. */
#ifdef HAVE_CFMAKERAW

    /* cfmakeraw unsets PARENB, and sets CS8 with the CSIZE mask */
    cfmakeraw(&stbuf);
    /* If CSTOPB is set two stop bits, if not then one */
    stbuf.c_cflag &= ~CSTOPB;

#else 

    stbuf.c_iflag = 0;
    stbuf.c_oflag = 0;
    stbuf.c_lflag = 0;

    stbuf.c_cflag &= ~PARENB;	/* no parity */
    stbuf.c_cflag &= ~CSTOPB;	/* one stop */
    stbuf.c_cflag &= ~CSIZE;	/* bits mask */

#endif

    stbuf.c_cflag |= bits | CLOCAL | CREAD;

    /* Setup to read one char at a time off the serial port */
    /*
     * Case D: VMIN = 0, VTIME = 0
     * The minimum of either the number of bytes requested or the number of
     * bytes currently available is returned without waiting for more bytes to
     * be input.  If no characters are available, read returns a value of zero,
     * having read no data.
     */
    stbuf.c_cc[VMIN]  = 0;
    stbuf.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &stbuf) < 0) {
	sprintf(errbuf, "%s: can't tcsetattr device %s", progname, device);
	perror(errbuf);
	close(fd);
	return(-3);
    }
#ifdef O_NONBLOCK
    /* Now that we have set CLOCAL on the port, we can use blocking I/O */
    flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#endif

    return(fd);
}

