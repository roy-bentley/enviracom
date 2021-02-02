/*
 *
 * Author: Scott R. Presnell
 *
 * Copyright (C) 2008
 */

/*
 * enviracom.h - General configuration information for the enviracom daemon
 *
 * Scott Presnell, srp@tworoads.net, Thu Aug 21 16:29:04 PDT 2008
 *
 */

/* maximum number of simultaneous client connections allowed by the server */
#define CONNECTIONS 128

/* usually a link to the actual serial device */
#define ENVIRACOM	"/dev/enviracom"

#define DATAPATH "/usr/local/etc/enviracom/data"

#define PORT	8587

/* Undef this if you do not have ualarm() library call */
#define HAVE_UALARM 1
#define HAVE_CFMAKERAW 1

extern char *progname;
extern char errbuf[];

extern int init_serial(char *);
extern void cleanup_serial(int);

