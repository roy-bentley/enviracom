/**********************************************************************************/
/**************** PVCS HEADER for Enviracom API Source Code ***********************/
/**********************************************************************************/
/*										  */
/* Archive: /Projects/Enviracom API/EnviracomAPIApp.c				  */
/* Date: 4/26/02 8:31a								  */
/* Workfile: EnviracomAPIApp.c							  */
/* Modtime: 12/21/01 11:32a							  */
/* Author: Snichols								  */
/* Log: /Projects/Enviracom API/EnviracomAPIApp.c				  */
/* 
 * 1     4/26/02 8:31a Snichols
 * Rev 1.02 - fixed bug so that it didn't check Setpoint Schedule every 90
 * minutes to see if it had been received.
 * 
 *          (do not change this line it is the end of the log comment)            */
/*                                                                                */

#ifndef lint
static char rcsid[] = "$Id: EnviracomAPIApp.c,v 1.5 2008/11/04 17:55:00 srp Exp srp $";
#endif

/*
 *
 * Author: Scott R. Presnell
 *
 * Copyright (C) 2008
 */

/*
 * encEnviracomAPIApp.c - daemon for listening to enviracom messages,
 *	holding current information, and responding to request for
 *	changes to the thermostat.
 *
 * Scott Presnell, srp@tworoads.net, Thu Aug 21 16:29:04 PDT 2008
 *
 */

/*
 * $Log: EnviracomAPIApp.c,v $
 * Revision 1.5  2008/11/04 17:55:00  srp
 * Moved VERSION into Makefile, added option to display version and build date.
 *
 * Revision 1.4  2008/11/04 17:24:35  srp
 * Added tracking of "recovery" status in setpoint packets.
 * Fixed resending of message when recv nack, or reset.
 * Note and show which scheduled periods are not set.
 * Added "OK"s after status replies.
 * Closed off getpwent() with endpwent(); getgrent() with endgrent().
 * Move to /var/tmp to allow for core dumps.
 *
 * Revision 1.3  2008/10/14 15:08:05  srp
 * Added conditional compilation called EXTENSIONS
 * Added elog for flexible logging to syslog, file, or stderr.
 * Wrapped debugging junk with debugging flag.
 * Added tracking of AllowedSystemSwitch
 * Added tracking/conversion of temperature units. Allow selection
 * based on thermostat setting, or forcing on command line.
 * Moved command parser out of main loop.
 *
 * Revision 1.2  2008/07/10 20:50:34  srp
 * Working Version: reads enviracom updates variables, writes to clients.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <signal.h>
#include <paths.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <errno.h>

#include "enviracom.h"
#include "elog.h"

#include "EnviracomAPI.h"

#define _ENVIRACOMAPIAPP_C_

/*
 * Commands:
 * F[an] [On[n]|Of[f]|A[uto] - control the fan
 * H[old] - hold the current setpoints.
 * Q[uery] - get a fixed report in cooked form.
 * L[ine] - get a fixed report in field form
 * M[ode] [H[eat]|C[ool]|O[ff]|A[uto]] - control the system switch
 * R[un] - run program - return to current setpoints, and execute
 * S[et] [H[eat]|C[ool]] temp[C|F] - create new setpoint, and execute
*/

/*
 * Others: 
 * A[llowed] modes?
 * T[ime]?
 * Current: heating|cooling|off?
*/


char errorstr[10][256] = {
    "ERROR: syntax: F[an] [A[uto]|On|Of[f]]\n",			/* 0 */
    "ERROR: bad argument to setFanSwitch\n",			/* 1 */
    "ERROR: syntax: M[ode] [A[uto]|H[eat]|C[ool]|O[ff]]\n",	/* 2 */
    "ERROR: bad argument to setSystemSwitch\n",			/* 3 */
    "ERROR: bad mode selected for setSystemSwitch\n",		/* 4 */
    "ERROR: syntax: S[et] [H[eat]|C[ool]] temp+units\n",	/* 5 */
    "ERROR: bad argument to NewSetPoint\n",			/* 6 */
    "WARN: setpoint temperature adjusted to limit\n",		/* 7 */
};

char okstr[10][256] = {
    "OK\n",							/* 0 */
};


char *helpstr = "Available Commands:\n\
	F[an] [On[n]|Of[f]|A[uto] - control the fan\n\
	H[old] - permanent hold the current setpoints.\n\
	Q[uery] - get a cooked report.\n\
	L[ine] - get a raw report.\n\
	M[ode] [H[eat]|C[ool]|O[ff]|A[uto]] - control the system switch\n\
	R[un] - run program - return to scheduled setpoints, and execute\n\
	S[et] [H[eat]|C[ool]] temp+units - create new setpoint, and temporary hold\n";


char Modes[5][10] = {
    "Em. Heat",
    "Heat",
    "Off",
    "Cool",
    "Auto",
};

char SetStatus[5][10] = {
    "Scheduled",
    "Temporary",
    "Hold",
    "Unknown",
    "Recovery",
};

char Days[7][4] = {
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat",
    "Sun",
};

char Periods[4][7] = {
    "Wake",
    "Leave",
    "Return",
    "Sleep",
};

char sUnits[2] = {
    'F', 'C',
};

char lUnits[2][15] = {
    "Fahrenheit",
    "Centigrade",
};

char AllowedModes[80]; /* determined from enviracom response */

/* Determined from enviracom response */
/* Pretty sure the default is Centigrade (zoneUnit is 1) */
int zoneUnits[N_ENVIRACOM_BUSSES][N_ZONES] = {
    1,
    1,
    1,
    1,
    1,
    1,
};

#if DEBUG
int	debug = 0;
#endif

/* Currently limited to one Enviracom Bus */
int	EnviracomBus = 0;

int	fd[CONNECTIONS];	/* file descriptors */
int	sd = -1;		/* serial port descriptor */
int	ld = -1;		/* internet socket descriptor */
int	maxfd = 2;		/* keep track of max file descriptor for select() */
int	cid = -1;		/* currently can only handle one child at a time */
int	af = -1;		/* ASCII file descriptor for logging data stream */

char *progname;

/* ASCII log filename */
char afilename[MAXPATHLEN];

char errbuf[128];			/*  for logging startup errros */

int *current, *max;
int alarmed = 0;

char *logfile = "syslog";

sigset_t aset; /* all set */
sigset_t bset; /* blocked set */
sigset_t eset; /* empty set */

void comm_log(int fd, struct tm *tlocal, char *message);

void parseCommand(char *command);

int setFanSwitch(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone, char *var);
int setSystemSwitch(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone, char *var);
int newSetPoint(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone, char *mode, float temp, char units);

char * convUnits(char *, int length, float temp, int units);

void
alarm_handler(int sig) 
{
/*    fputs("sigalarm!\n", stderr);*/
    alarmed = 1;
}

void
pipe_handler(int sig)                /* SIGPIPE signal handler */
{
  signal(SIGPIPE, SIG_IGN);
  if (sig == SIGPIPE) {         /* client went away */
    elog(ELOG_WARNING, "shutdown and close socket %d from SIGPIPE", *current);
    shutdown(*current, 2);
    close(*current);
    *current = -1;
  }
}

void
die_handler(int sig)                /* SIGPIPE signal handler */
{
    elog(ELOG_WARNING, "shutdown and close internet socket");
    shutdown(ld, 2);
    close(ld);
    cleanup_serial(sd);
//    close(sd);
    exit(1);
}

void
wait_handler(int sig) 
{

    union wait status;
    int pid;

    while ((pid = wait3((int *) &status, WNOHANG, (struct rusage *)0)) > 0 && pid != cid) {
	;
    }

/*    syslog(LOG_INFO, "parent: cool - got a child %d", pid);*/

    cid = 0;

}

#if !defined(HAVE_UALARM)

#define USPS    1000000         /* # of microseconds in a second */

/*
 * Generate a SIGALRM signal in ``usecs'' microseconds.
 * If ``reload'' is non-zero, keep generating SIGALRM
 * every ``reload'' microseconds after the first signal.
 */
int
ualarm(unsigned long usecs, unsigned long reload)
{
        struct itimerval new, old;

        new.it_interval.tv_usec = reload % USPS;
        new.it_interval.tv_sec = reload / USPS;
        
        new.it_value.tv_usec = usecs % USPS;
        new.it_value.tv_sec = usecs / USPS;

        if (setitimer(ITIMER_REAL, &new, &old) == 0)
                return (old.it_value.tv_sec * USPS + old.it_value.tv_usec);
        /* else */
                return (-1);
}
#endif

usage()
{
  printf("usage: %s [opts] [-p pidfile] [-P port] [-s device] [-w path] [-u usr:grp]\n\
  -A		don't write ASCII enviracom data logs \n\
  -C		force units to Centigrade \n\
  -F		force units to Fahrenheit \n\
  -d		debug; don't fork to be a daemon \n\
  -h		show this help and exit \n\
  -l file	syslog facility or logfile \n\
  -e		enter daily records in SQL database \n\
  -p file	pid file \n\
  -P port       listen for client connections on port instead of %d \n\
  -s device     get enviracom data from device instead of %s \n\
  -w path       write files in path instead of %s \n\
  -V		version information\n\
  -Z		don't gzip daily data files after collection \n",
	 progname, PORT, ENVIRACOM, DATAPATH);
  exit(0);
}


main(int argc, char **argv)
{
    int i, f, rtn, pid;
    int clen;
    int sw_gid, sw_uid;
    int pid_fd, null_fd;

    int opt_P = PORT;
    int afile = -1, c, first = 1;
    int pday = -1, pmon = -1, pwday = -1;
    int opt_a = 1, opt_z = 1;
    int opt_d = 0, opt_e = 0, opt_r = 0, opt_u = 0;

    struct group *gr;
    struct passwd *pw;

    char *opt_s = ENVIRACOM, *opt_w = DATAPATH;
    char *opt_p;
    char *tptr, *endp;
    char *user, *group;
    char readchar;
    char *pid_file;
    char tempbuf[80];
    char command[MAXPATHLEN];
    char sbuffer[BUFSIZ];
    char tbuffer[BUFSIZ];
    int  sbuflen = BUFSIZ -1;

    time_t now;
    struct tm *tlocal;
    struct timespec tv;

    struct sockaddr_in server, client;
    struct in_addr bind_address;
    fd_set rfds;

    progname = strrchr(argv[0], '/');

    if (progname == NULL)
	progname = argv[0];         /* global for error messages, usage */
    else
	progname++;

    while ((c = getopt(argc, argv, "ACFVZdehrl:p:s:w:u:")) != EOF) {
	switch(c) {
	case 'A':
	    opt_a = 0;
	    break;
	case 'C':
	    zoneUnits[0][0] = 1; /* FIXME: only setting bus 0, zone 0 */
	    break;
	case 'F':
	    zoneUnits[0][0] = 0; /* FIXME: only setting bus 0, zone 0 */
	    break;
	case 'Z':		/* do not gzip nightly after collection */
	    opt_z = 0;
	    break;
	case 'd':		/* debug: also do not fork and become a daemon */
	    opt_d = 1;
	    logfile = "stderr";
	    break;
	case 'e':
	    opt_e = 1;
	    break;
	case 'r':		/* raw debugging */		
	    opt_r = 1;
	    break;
	case 'l':
	    logfile = optarg;
	    break;
	case 'p':		/* port to use */
	    pid_file = optarg;
	    break;
	case 'P':
	    opt_P = atoi(optarg);
	    break;
	case 's':		/* serial device to use */
	    opt_s = optarg;
	    break;
	case 'w':		/* path to write to */
	    opt_w = optarg;
	    break;
	case 'u':
	    user = malloc(strlen(optarg) + 1);
	    if (user != NULL) {
		(void)strncpy(user, optarg, strlen(optarg) + 1);
		if (!(group = strrchr(user, ':'))) {
		    group = strrchr(user, '.');
		}
		if (group)
		    *group++ = '\0'; /* get rid of the ':' */
		opt_u = 1;
	    } else {
		sprintf(errbuf, "%s: user malloc", progname);
		perror(errbuf);
		exit(1);
	    }
	    break;
	case 'V':
	    printf("Version: %s\nDate:    %s\n", VERSION, BUILD_DATE);
	    exit(0);
	case '?':
	case 'h':		/* help */
	    usage();
	    break;
	}
    }

    sigfillset(&aset);
    sigemptyset(&eset);
    sigemptyset(&bset);
    sigaddset(&bset, SIGALRM);
    sigaddset(&bset, SIGCHLD);

    /* initialize the serial file descriptor */
    if ((sd = init_serial(opt_s)) < 0)
	exit(sd);

    /* setup the internet socket */
    server.sin_family = AF_INET;
    bind_address.s_addr = htonl(INADDR_ANY);
    server.sin_addr = bind_address;
    server.sin_port = htons(opt_P);

    /* create ... */
    if ((ld = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
	sprintf(errbuf, "%s: socket", progname);
	perror(errbuf);
	cleanup_serial(sd);
	exit(10);
    }

    /* bind ... */
    if (bind(ld, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) == -1) {
	sprintf(errbuf, "%s: bind", progname);
	perror(errbuf);
	cleanup_serial(sd);
	exit(11);
    }

    /* and listen. */
    if (listen(ld, CONNECTIONS) == -1) {
	sprintf(errbuf, "%s: listen", progname);
	perror(errbuf);
	cleanup_serial(sd);
	exit(12);
    }

    for (i = 0; i < CONNECTIONS; i++)
	fd[i] = -1;

    umask(0022);

    if (!opt_d) {
	/* we're initialized; now become a daemon */
	if ((pid = fork()) == -1) {
	    sprintf(errbuf, "%s: fork", progname);
	    perror(errbuf);
	    cleanup_serial(sd);
	    exit(20);
	} else if (pid) {
	    /* parent exits*/
	    exit (0);
	}
	/* child lives on */

	/* set the session */
	if (setsid() == -1) {
	    sprintf(errbuf, "%s: setsid", progname);
	    perror(errbuf);
	    cleanup_serial(sd);
	    exit(21);
	}

	if (chdir(_PATH_VARTMP) == -1) {
	    sprintf(errbuf, "%s: chdir %s", progname, _PATH_VARTMP);
	    perror(errbuf);
	    cleanup_serial(sd);
	    exit(22);
	}

        if ((null_fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
	    (void)dup2(null_fd, STDIN_FILENO);
	    (void)dup2(null_fd, STDOUT_FILENO);
	    (void)dup2(null_fd, STDERR_FILENO);
	}
#if 0
	/* close up unused files */
	for (i = 0; i < NOFILE; i++)
	    if (i != sd && i != ld) close(i);
#endif
    }

    if (pid_file) {
	if ((pid_fd = open(pid_file, (O_RDWR|O_CREAT|O_TRUNC), 0644)) < 0) {
	    perror("cannot open pid file: ");
	    fputs(pid_file, stderr);
	    exit(2);
	}
	sprintf(tempbuf, "%d", (int) getpid());
	write(pid_fd, tempbuf, strlen(tempbuf));
	close(pid_fd);
    }

    /* open up a logfile, or syslog */
    switch_to_logfile(logfile);

    /* become a daemon */
    if (!opt_d && opt_u) {

	if (group != NULL) {
	    if (isdigit((unsigned char)*group)) {
		sw_gid = (gid_t)strtoul(group, &endp, 0);
		if (*endp != '\0') 
		    goto getgroup;
	    } else {
    getgroup:	
		if ((gr = getgrnam(group)) != NULL) {
		    sw_gid = gr->gr_gid;
		} else {
		    errno = 0;
		    elog(ELOG_ERR, "Cannot find group `%s'", group);
		    exit (-1);
		}
		endgrent(); /* close the filehandle */
	    }
	    setregid(sw_gid, sw_gid);

	}

	if (user != NULL) {
	    if (isdigit((unsigned char)*user)) {
		sw_uid = (uid_t)strtoul(user, &endp, 0);
		if (*endp != '\0') 
		    goto getuser;
	    } else {
    getuser:	
		if ((pw = getpwnam(user)) != NULL) {
		    sw_uid = pw->pw_uid;
		} else {
		    errno = 0;
		    elog(ELOG_ERR, "Cannot find user `%s'", user);
		    exit (-1);
		}
		endpwent(); /* close the filehandle */
	    }
	    setreuid(sw_uid, sw_uid);
	}
    }

    /* no signals are masked */
    sigprocmask(SIG_SETMASK, (sigset_t *) &eset, (sigset_t *) NULL);

    signal(SIGINT, die_handler);
    signal(SIGTERM, die_handler);
    signal(SIGCHLD, wait_handler);

    elog(ELOG_INFO, "version %s ready", VERSION);

    /* Initialize the Enviracom API */
    EnvrcmInit();

    /* (re)set the string buffer for ASCII output */
    tptr = tbuffer;

    while (1) {

	alarmed = 0;
	/* One second, 1,000,000 micro seconds */
        ualarm(1000000, 0);

        signal(SIGALRM, alarm_handler);

	/* main part of listener */
    loop:

	FD_ZERO(&rfds);
	FD_SET(sd, &rfds);
	FD_SET(ld, &rfds);

	/* Queue up the active sockets as well */
	for (current = fd; current <= max; current++) {
	    if (*current > 0) {
		FD_SET(*current, &rfds);
	    }
	}

	/* Set Up for a Poll */
	tv.tv_sec = 0;
	tv.tv_nsec = 0;
	errno = 0;
	f = 0;
	rtn = 1;

	/* Find the highest open file dscriptor. */
	maxfd = find_maxfd(ld);


	/* Don't allow SIGALRM or SIGCHLD to interrupt us */
	while ((f = pselect(maxfd + 1, &rfds, NULL, NULL, &tv, &bset)) > 0) {

	    /* Read the serial port */	
	    if (FD_ISSET(sd, &rfds)) {
		rtn = readbyte(sd, &readchar, 1);
		if (rtn == 1)  {
		    if (readchar == '\r') {
#ifdef DEBUG
			if (opt_d && opt_r)
			fputc('\n', stderr);
#endif
			if (opt_a) {
			    *tptr++ = '\n';
			}
		    } else if (readchar == '\n') {
#ifdef DEBUG
			if (opt_d && opt_r) {
			    fputc('#', stderr);
			}
#endif
		    } else {

			if (readchar != '>') {
#ifdef DEBUG
			    if (opt_d && opt_r) {
				fputc(readchar, stderr);
			    }
#endif
			    if (opt_a && isascii(readchar)) {
				*tptr++ = readchar;
			    }
			}
		    }

		    /*
		     * Protect API function calls block ALRM and CHLD - not sure if this is nessarary 
		     */
		    sigprocmask(SIG_BLOCK, &bset, (sigset_t *) NULL);
		    NewEnvrcmRxByte(readchar, EnviracomBus);
		    sigprocmask(SIG_UNBLOCK, &bset, (sigset_t *) NULL);

		}
		--f; /* remove serial descriptor from count */
	    } 

	    /* Check for new client connections */
	    if (FD_ISSET(ld, &rfds)) {
		for (current = fd; (*current > 0) && (current < fd + CONNECTIONS-1); current++);
		if (current >= max) 
		    max = current;
		clen = sizeof(struct sockaddr_in);
		*current = accept(ld, (struct sockaddr *)&client, &clen);
		fprintf(stderr, "got a new socket: %d\n", *current);

		maxfd = find_maxfd(maxfd);

		--f; /* remove new connections from count */
	    }

	    /* If there are still sockets to be read, they are attached clients... */
	    if (f > 0) {

		/* Only write() on a closed socket generates SIGPIPE, not read(). */
		signal(SIGPIPE, pipe_handler);
		for (current = fd; current <= max; current++) {
		    if (*current > 0 && FD_ISSET(*current, &rfds) ) {
			rtn = readbyte(*current, sbuffer, sbuflen);
			if (rtn > 0)  {
#ifdef DEBUG
			    if (opt_d) {
				elog(ELOG_INFO, "client command: \"%c\"", sbuffer[0]);
			    }
#endif
			    parseCommand(sbuffer);
			} else {
			    elog(ELOG_INFO, "shutdown and close socket %d from writer", *current);
			    shutdown(*current, 2);
			    close(*current);
			    *current = -1;
			    maxfd = find_maxfd(maxfd);
			}
		    }
		}
		signal(SIGPIPE, SIG_IGN);
	    }

	    maxfd = find_maxfd(maxfd);
	    if (alarmed) break;
	}

	if (!alarmed) {
	    /* wait now until the alarm goes off or we get some other signal */
	    sigsuspend((sigset_t *) NULL);
	}

	if (alarmed) {

	    Envrcm1Sec();

	} else { /* It was some other signal, sigchld only?  alarmed is still 0 */

	    /* alarm still pending */
	    goto loop;
	}

	/* Log the commands over Enviracom net to the ASCII logfile */
	if (opt_a) {
	    now = time(NULL);
	    tlocal = localtime(&now);

	    /* Open a file for writing if first time or new day */
	    if ((tlocal->tm_mday != pday) || first) {
		if (!first) {         /* non-first files? close current ones */
		    if (af > -1) close(af);
		    if (opt_z) {       /* compress yesterday's files */
			if (af > -1)
			    sprintf(command, "gzip %s &", afilename);
			system(command);
		    }
		}
		sprintf(afilename, "%s/%02d%02d%02d.tab", opt_w,
			1900 + tlocal->tm_year, tlocal->tm_mon+1, tlocal->tm_mday);

		if ((af = open(afilename, O_WRONLY|O_CREAT|O_APPEND, 00644)) < 0) {
		    elog(ELOG_ERR, "can't open %s: %m", afilename);
		    cleanup_serial(sd);		
		    exit(1);
#ifdef DEBUG
		} else {
		    if (opt_d) {
			elog(ELOG_DEBUG, "opened \"%s\" as %d", afilename, af);
		    }
#endif
		}

		pday = tlocal->tm_mday;
		pmon = tlocal->tm_mon+1;
		pwday = tlocal->tm_wday;
		first = 0;
	    }

	    /* Write out stamped message to ASCII log file */
	    if (af && *(tptr-1) == '\n') {
		*tptr = '\0';
		comm_log(af, tlocal, tbuffer);
		tptr = tbuffer;
	    } /* else continue to accumulate */
	}
    }

    cleanup_serial(sd);
    exit(0);

}


/*
 * Read N bytes from filedescriptor, blocking SIGALRM and SIGCHLD
*/
int
readbyte(int fd, char * buf, int maxlen)
{
    
    int rtn = 0, i = 0;

    errno = 0;

    sigprocmask(SIG_BLOCK, &bset, (sigset_t *) NULL);
    rtn = read(fd, (void *) buf, maxlen);
    sigprocmask(SIG_UNBLOCK, &bset, (sigset_t *) NULL);

    if (rtn < 0) {
	/* should not be getting a blocked read here */
	elog(ELOG_INFO, "error in reading %d of %d: %m", rtn, maxlen);
	return(-1);
    }
    return rtn;
}

/*
 * Find the highest filedescriptor amongst the connected clients.
*/
int
find_maxfd(int mfd)
{

    int *current;

    for (current = fd; (*current > 0) && (current < fd + CONNECTIONS-1); current++) {
	if (*current > mfd) {
	    mfd = *current;
	}
    }
    return mfd;
}

void comm_log(int fd, struct tm *tlocal, char *message)
{
    char buffer[BUFSIZ];
    char *ptr, *bptr;
    float timestamp;
    int c = 0;

    if (ptr = strtok(message, "\n")) {

	timestamp = tlocal->tm_hour + ((((float) tlocal->tm_min * 60) + (float) tlocal->tm_sec)/3600);

	do {	

	    if (strstr(ptr, "Idle") || strstr(ptr, "Ack")) {
		break;
	    }

	    if (strstr(ptr, "Reset") || strstr(ptr, "Nack")) {
		elog(ELOG_INFO, "saw %s", ptr);
	    }

	    sprintf(buffer, "%.4f", timestamp);
	    bptr = &buffer[strlen(buffer)];
	    *bptr++ = '\t';
	    
	    while (*bptr++ = *ptr++);
	    --bptr;
	    *bptr++ = '\n';
	    *bptr = '\0';

	    if (write(fd, buffer, strlen(buffer)) < 0) {
		elog(ELOG_ERR, "can't write %s: %m", afilename);
		return;
	    }
	    
	} while (ptr = strtok(NULL, "\n"));
    }
}

void
parseCommand(char *command)
{


    /* Arguments for reading commands from clients */
    char *ptr;
    char heat[10], cool[10], room[10], outdoor[10];

    char cmd[128], arg1[128], arg2[128], arg3[128];
    float stemp;
    char utemp;
    int rtn;


    (void) convUnits(heat, 10, (Envrcm_HeatSetpoint[0][0] / 100.0), zoneUnits[0][0]);
    (void) convUnits(cool, 10, (Envrcm_CoolSetpoint[0][0] / 100.0), zoneUnits[0][0]);
    (void) convUnits(room, 10, (Envrcm_RoomTemperature[0][0] / 100.0), zoneUnits[0][0]);
    (void) convUnits(outdoor, 10, (Envrcm_OutdoorTemperature / 100.0), zoneUnits[0][0]);
    
    if (toupper(command[0]) == 'Q') {

	char qbuffer[255];
	int  qbuflen;
	/*
	 * bus, zone, sysswitch, fan switch, set status,
	 * heat set, cool set, indoor temp, outdoor temp, humidity %
	 * airfilter remaining, airfilter restart
	 */
	sprintf(qbuffer, "Bus:%d, Zone:%d, Mode:%s, Fan:%s, SetStatus:%s, Heat:%s, Cool:%s, Room:%s, Outside:%s, Humidity:%d%%, Filter:%d, Filter:%d\n",
		0, 0,
		Modes[(Envrcm_SystemSwitch[0][0] & 0x7F)],
		(Envrcm_FanSwitch[0][0] > 0) ? "On" : "Auto",
		SetStatus[Envrcm_SetpointStatus[0][0]],
		heat, cool, room, outdoor,
		Envrcm_Humidity[0],
		Envrcm_AirFilterRemain[0],
		Envrcm_AirFilterRestart[0]);

	qbuflen = strlen(qbuffer);
	write(*current, qbuffer, qbuflen);
	write(*current, okstr[0], strlen(okstr[0]));
    } else if (toupper(command[0]) == 'L') { /*dont broadcast? */

	char lbuffer[80];
	int  lbuflen;
	/*
	 * bus, zone, sysswitch, fan switch, set status,
	 * heat set, cool set, indoor temp C, outdoor temp C, humidity %
	 * airfilter remaining, airfilter restart
	 */
	sprintf(lbuffer, "%d %d %d %d %d %s %s %s %s %d %d %d\n",
		0, 0,
		(Envrcm_SystemSwitch[0][0] & 0x7F),
		Envrcm_FanSwitch[0][0],
		Envrcm_SetpointStatus[0][0],
		heat, cool, room, outdoor,
		Envrcm_Humidity[0],
		Envrcm_AirFilterRemain[0],
		Envrcm_AirFilterRestart[0]);

	lbuflen = strlen(lbuffer);
	write(*current, lbuffer, lbuflen);
	write(*current, okstr[0], strlen(okstr[0]));
    } else if (toupper(command[0]) == 'F') {
	if (sscanf(command, "%s %s", &cmd, &arg1) != 2) {
	    write(*current, errorstr[0], strlen(errorstr[0]));
	} else {
	    if (setFanSwitch(0, 0, arg1) == -1) {
		write(*current, errorstr[1], strlen(errorstr[1]));
	    } else {
		write(*current, okstr[0], strlen(okstr[0]));
	    }
	}
    } else if (toupper(command[0]) == 'M') {
	if (sscanf(command, "%s %s", &cmd, &arg1) != 2) {
	    write(*current, errorstr[2], strlen(errorstr[2]));
	} else {
	    if ((rtn = setSystemSwitch(0, 0, arg1)) == -1) {
		write(*current, errorstr[3], strlen(errorstr[3]));
	    } else if (rtn == -2) {
		write(*current, errorstr[4], strlen(errorstr[4]));
		write(*current, AllowedModes, strlen(AllowedModes));
		write(*current, "\n", 1);
	    } else {
		write(*current, okstr[0], strlen(okstr[0]));
	    }
	}
    } else if (toupper(command[0]) == 'R') {
	RunProgram(0, 0);	/* void return --- FIXME? -- */
	write(*current, okstr[0], strlen(okstr[0]));
    } else if (toupper(command[0]) == 'S') {
	if (sscanf(command, "%s %s %f%c", &cmd, &arg1, &stemp, &utemp) != 4) {
	    write(*current, errorstr[5], strlen(errorstr[5]));
	} else {
	    if ((rtn = newSetPoint(0, 0, arg1, stemp, utemp)) == -1) {
		write(*current, errorstr[6], strlen(errorstr[6]));
	    } else if (rtn > 0) {
		write(*current, errorstr[7], strlen(errorstr[7]));
	    } else {
		write(*current, okstr[0], strlen(okstr[0]));
	    }
	}
    } else if (toupper(command[0]) == 'H') {
	HoldSetpoints(0, 0);	/* void return --- FIXME? -- */
	write(*current, okstr[0], strlen(okstr[0]));
    } else if (toupper(command[0]) == 'X') {
	write(*current, okstr[0], strlen(okstr[0]));
	shutdown(*current, 2);
	close(*current);
	*current = -1;
	maxfd = find_maxfd(maxfd);
    } else {
	write(*current, helpstr, strlen(helpstr));
	write(*current, okstr[0], strlen(okstr[0]));
    }

}
	
int
setFanSwitch(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone, char *var)
{

    int setting = 0;

    if (toupper(var[0]) == 'A') {
	setting = 0;
    } else if (toupper(var[1]) == 'N') {
	setting = 200;
    } else if (toupper(var[1]) == 'F') {
	setting = 0;
    } else {
	elog(ELOG_WARNING, "setFanSwitch: unknown setting \"%s\"\n", var);
	return (-1);
    }
    
    Envrcm_FanSwitch[EnviracomBus][Zone] = setting;
    ChangeFanSwitch(EnviracomBus, Zone);
}

int
setSystemSwitch(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone, char *var)
{

    int setting = 0;

    if (toupper(var[0]) == 'A') {
	setting = 4;
    } else if (toupper(var[0]) == 'C') {
	setting = 3;
    } else if (toupper(var[0]) == 'O') {
	setting = 2;
    } else if (toupper(var[0]) == 'H') {
	setting = 1;
    } else {
	elog(ELOG_WARNING, "setSystemSwitch: unknown setting \"%s\"\n", var);
	return (-1);
    }

    // 1  Bit 0 = Emergency Heat
    // 2  Bit 1 = Heat
    // 4  Bit 2 = Off
    // 8  Bit 3 = Cool
    // 16 Bit 4 = Auto

    if (((Envrcm_AllowedSystemSwitch[EnviracomBus][Zone] & 0x02) && setting == 1) ||
	((Envrcm_AllowedSystemSwitch[EnviracomBus][Zone] & 0x04) && setting == 2) ||
	((Envrcm_AllowedSystemSwitch[EnviracomBus][Zone] & 0x08) && setting == 3) ||
	((Envrcm_AllowedSystemSwitch[EnviracomBus][Zone] & 0x10) && setting == 4)) {

	Envrcm_SystemSwitch[EnviracomBus][Zone] = (setting & 0x7F);
	ChangeSystemSwitch(EnviracomBus, Zone);

    } else {
	elog(ELOG_WARNING, "setSystemSwitch: bad choice \"%s\"\n", var);
	return (-2);
    }

}

int
newSetPoint(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone, char * mode, float temp, char units)
{

    int rtn = 0;

    /* If Fahrenheit - convert to centigrade */
    if (toupper(units) == 'F') {
	temp -= 32;
	temp *= 0.55556;
    }

    /* If cool requested, change that... */
    if (toupper(mode[0]) == 'C') {

	    Envrcm_CoolSetpoint[EnviracomBus][Zone] = temp * 100;
	    rtn = ChangeCoolSetpoint(EnviracomBus, Zone);

    /* If heat requested, change that... */
    } else if (toupper(mode[0]) == 'H') {

	    Envrcm_HeatSetpoint[EnviracomBus][Zone] = temp * 100;
	    rtn = ChangeHeatSetpoint(EnviracomBus, Zone);

    } else {
	return (-1);
    }

    return (rtn);
}

/*
 * Convert units appropriately.
 * Within the application,  temperature is always in Centigrade
 * so only convert if requesting Fahrenheit
*/
char *
convUnits(char * out, int len, float temp, int units) {

    /* make then Fahrenheit */
    if (units == 0) {
	temp *= 1.8;
	temp += 32;
    }

    (void) snprintf(out, len, "%.1f%c", temp, sUnits[units]);

    return out;

}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Notification Functions
//////////////////////////////////////////////////////////////////////////////////////////////////

// This function is called by the API whenever an Air Filter Timer message is received
// on Enviracom whose value differs from what is in Envrcm_AirFilterRemain and/or 
// Envrcm_AirFilterRestart.  EnviracomBus indicates which Enviracom bus has the value that
// changed.  The variables Envrcm_AirFilterRemain and Envrcm_AirFilterRestart will contain
// the new values from the received message.  This function can do whatever the application 
// requires.
void App_AirFilterNotify(UNSIGNED_BYTE EnviracomBus)
{
    elog(ELOG_INFO, "AirFilter: Bus %d: %d fan-days remain",
	    EnviracomBus, Envrcm_AirFilterRemain[EnviracomBus]);

    /* , %d fan-days at reset", */
    /* Envrcm_AirFilterRestart[EnviracomBus]);*/
}

// This function is called by the API whenever an Ambient Humidity message is received
// on Enviracom whose value differs from what is in Envrcm_Humidity.  EnviracomBus 
// indicates which Enviracom bus has the value that changed.    The variable Envrcm_Humidity
// will contain the new value from the received message.  This function can 
// do whatever the application requires.
void App_HumidityNotify(UNSIGNED_BYTE EnviracomBus)
{
    elog(ELOG_INFO, "Humidity: Bus %d is %dpct", EnviracomBus, Envrcm_Humidity[EnviracomBus]);
}


#ifdef EXTENSIONS
void App_OutdoorTempNotify(UNSIGNED_BYTE EnviracomBus)
{
    char tempbuf[80];

    /* FIXME: using Zone 0 setting on this bus */
    elog(ELOG_INFO, "OutTemp:  Bus %d is %s", EnviracomBus,
	 convUnits(tempbuf, sizeof(tempbuf), (Envrcm_OutdoorTemperature) / 100.0, zoneUnits[EnviracomBus][0]));
}
#endif

// this function is called by the API whenever a message is received on Enviracom whose
// value differs from what is in Envrcm_HeatPumpFault.  EnviracomBus indicates which 
// Enviracom bus has the value that changed.  The variable Envrcm_HeatPumpFault will contain
// the new value in the received message. This function can do whatever the
// application requires.
void App_HeatPumpFaultNotify(UNSIGNED_BYTE EnviracomBus)
{
    if (Envrcm_HeatPumpPresent[EnviracomBus] != -1) {
	elog(ELOG_WARNING, "HeatPumpFault: Bus %d => Present: %d; Fault: %d",
	     EnviracomBus, Envrcm_HeatPumpPresent[EnviracomBus], Envrcm_HeatPumpFault[EnviracomBus]);
    }
}

#ifdef EXTENSIONS

// This function is called by the API whenever a message is received on Enviracom indicating
// that the state of the heatpump/furnace fan has changed. 
// Read Only; This is experimental.
void App_HeatPumpFanNotify(UNSIGNED_BYTE EnviracomBus)
{
    if (Envrcm_HeatPumpFan[EnviracomBus] != -1) {
	elog(ELOG_INFO, "Equipment Fan: Bus %d is %d", EnviracomBus, Envrcm_HeatPumpFan[EnviracomBus]);
    }
}

// This function is called by the API whenever a message is received on Enviracom indicating
// that the state of the heatpump/furnace stage has changed.  This is experimental.
// Read Only; This is experimental.
void App_HeatPumpStageNotify(UNSIGNED_BYTE EnviracomBus)
{
    if (Envrcm_HeatPumpStage[EnviracomBus] != -1) {
	elog(ELOG_INFO, "Equipment Stage: Bus %d is %d", EnviracomBus, Envrcm_HeatPumpStage[EnviracomBus]);
    }
}
#endif

// This function is called by the API whenever a message is received on Enviracom whose
// value differs from what is in Envrcm_HeatFault or Envrcm_CoolFault.  EnviracomBus 
// indicates which Enviracom bus has the value that changed.  The variables Envrcm_HeatFault
// and Envrcm_CoolFault will contain the new values from the received message.  This function 
// can do whatever the application requires.
void App_EquipFaultNotify(UNSIGNED_BYTE EnviracomBus)
{
    elog(ELOG_WARNING, "Equipment Fault: Bus %d", EnviracomBus);
}


// This function is called by the API whenever a message is received
// on Enviracom whose value differs from what is in Envrcm_RoomTemperature or 
// Envrcm_RoomTempUnits.  EnviracomBus and Zone indicate which Enviracom bus and which
// zone has the value that changed.  The variables Envrcm_RoomTempUnits and 
// Envrcm_RoomTemperature will contain the new values from the received message.  This 
// function can do whatever the application requires.
void App_RoomTempNotify(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone)
{
    char tempbuf[80];

    if (zoneUnits[EnviracomBus][Zone] != Envrcm_RoomTempUnits[EnviracomBus][Zone]) {

	zoneUnits[EnviracomBus][Zone] = Envrcm_RoomTempUnits[EnviracomBus][Zone];

	elog(ELOG_INFO, "TempUnit: Bus %d: Zone %d is %s", EnviracomBus, Zone,
		lUnits[zoneUnits[EnviracomBus][Zone]]);
    }

    /* signed word value */
    if (Envrcm_RoomTemperature[EnviracomBus][Zone] == 0x7FFF) {
	elog(ELOG_WARNING, "RoomTemp: Bus %d: Zone %d => N/A", EnviracomBus, Zone);
    } else if (Envrcm_RoomTemperature[EnviracomBus][Zone] == 0x8000) {
	elog(ELOG_WARNING, "RoomTemp: Bus %d: Zone %d is in fault", EnviracomBus, Zone);
    } else {
	elog(ELOG_INFO, "RoomTemp: Bus %d: Zone %d is %s", EnviracomBus, Zone,
	     convUnits(tempbuf, sizeof(tempbuf), (Envrcm_RoomTemperature[EnviracomBus][Zone] / 100.0), zoneUnits[EnviracomBus][Zone]));
    }

}


// This function is called by the API whenever a message is received
// on Enviracom whose value differs from what is in Envrcm_FanSwitch.  EnviracomBus and Zone
// indicate which Enviracom bus and which zone has the value that changed.  The variable
// Envrcm_FanSwitch will contain the new value from the received message.  This function
// can do whatever the application requires.
void App_FanSwitchNotify(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone)
{
    elog(ELOG_INFO, "FanSwitch: Bus %d: Zone %d is %d (%s)",
	 EnviracomBus, Zone, Envrcm_FanSwitch[EnviracomBus][Zone],
	 (Envrcm_FanSwitch[EnviracomBus][Zone] ? "On" : "Auto"));
}


// This function is called by the API whenever a message is received
// on Enviracom whose value differs from what is in Envrcm_HeatSetpoint, Envrcm_CoolSetpoint,
// and/or Envrcm_SetpointStatus.  EnviracomBus and Zone indicate which Enviracom bus and 
// which zone has the value(s) that changed.  The variables Envrcm_HeatSetpoint, 
// Envrcm_CoolSetpoint, and Envrcm_SetpointStatus will contain the new values from the 
// received message.  This function can do whatever the application requires.
void App_SetpointsNotify(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone)
{

    char *ptr, heat[10], cool[10];

    (void) convUnits(heat, 10, (Envrcm_HeatSetpoint[EnviracomBus][Zone] / 100.0), zoneUnits[0][0]);
    (void) convUnits(cool, 10, (Envrcm_CoolSetpoint[EnviracomBus][Zone] / 100.0), zoneUnits[0][0]);

    elog(ELOG_INFO, "SetPoints: Bus %d: Zone %d: Status: %d (%s) => Heat: %s Cool: %s",
	 EnviracomBus, Zone,
	 Envrcm_SetpointStatus[EnviracomBus][Zone],
	 SetStatus[Envrcm_SetpointStatus[EnviracomBus][Zone]],
	 heat, cool);
}


#ifdef SCHEDULESIMPLEMENTED
// This function is called by the API whenever a 
// message is received on Enviracom whose value differs from what is in 
// Envrcm_ScheduleStartTime, Envrcm_ScheduleHeatSetpoint, and/or Envrcm_ScheduleCoolSetpoint.  
// EnviracomBus, Zone, and PeriodDay indicate which Enviracom bus/zone/period/day has the 
// value(s) that changed.  The variables Envrcm_ScheduleStartTime, 
// Envrcm_ScheduleHeatSetpoint, and Envrcm_ScheduleCoolSetpoint will contain the new values
// from the received message.  This function can do whatever the application requires.
// Bits 0-3:	Day, 0 = Mon, 1 = Tue, . . ., 6 = Sun.  All other values are not
//		allowed.
// Bits 4-8:	Period, 0 = Wake, 1 = Leave, 2 = Return, 3 = Sleep.  All other values 
//		are not allowed.
// 07XXh = not scheduled.  The period does not occur for the day and zone.
// 08XXh = unknown (READ ONLY)
// 09XXh = the thermostat did not respond to the query (READ ONLY)

void App_ScheduleNotify(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone, UNSIGNED_BYTE PeriodDay)
{
    char *ptr, heat[10], cool[10];
    int Day;
    int Period;
    int StartTime;
    int HStart;
    int MStart;

    Day = (int) (PeriodDay & 0x0F);
    Period = (int) (PeriodDay >> 4);

    StartTime = Envrcm_ScheduleStartTime[EnviracomBus][Zone][Day][Period];
    if ((StartTime & 0x0F00) == 0x0800) { /* Unknown */
	HStart = -1;
	MStart = -1;
	ptr = "Unknown";
    } else if ((StartTime & 0x0F00) == 0x0700) { /* Not Scheduled */
	HStart = -2;
	MStart = -2;
	ptr = "Not Scheduled";
    } else {
	HStart = (int) StartTime / 60;
	MStart = (int) StartTime % 60;
    }

    (void) convUnits(heat, 10, (Envrcm_ScheduleHeatSetpoint[EnviracomBus][Zone][Day][Period] / 100.0), zoneUnits[0][0]);
    (void) convUnits(cool, 10, (Envrcm_ScheduleCoolSetpoint[EnviracomBus][Zone][Day][Period] / 100.0), zoneUnits[0][0]);

    if (HStart >= 0) {
	elog(ELOG_INFO, "Schedule: Bus %d: Zone %d: Day %d (%s): Period %d (%s): Heat: %s Cool: %s @ %02d:%02d",
	     EnviracomBus, Zone, Day, Days[Day], Period, Periods[Period], heat, cool, HStart, MStart);
    } else {
	elog(ELOG_INFO, "Schedule: Bus %d: Zone %d: Day %d (%s): Period %d (%s): %s",
	     EnviracomBus, Zone, Day, Days[Day], Period, Periods[Period], ptr);
    }
}
#endif


// This function is called by the API whenever a message is received
// on Enviracom whose value differs from what is in Envrcm_SystemSwitch.  EnviracomBus and 
// Zone indicate which Enviracom bus and which zone has the value that changed.  The variable
// Envrcm_SystemSwitch and Envrcm_AllowedSystemSwitch will contain the new values from the 
// received message.  This function can do whatever the application requires.
void App_SysSwitchNotify(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone)
{
    char *ptr;

    int currentMode = Envrcm_SystemSwitch[EnviracomBus][Zone] & 0x7F;

    elog(ELOG_INFO, "System Switch: Bus %d: Zone %d is %d (%s)",
	    EnviracomBus, Zone, currentMode, Modes[currentMode]);

    ptr = &AllowedModes[0];
    *ptr = '\0';

    /* Off */
    if (Envrcm_AllowedSystemSwitch[EnviracomBus][Zone] & 0x04) {
	ptr = strcat(ptr, Modes[2]);
    }

    /* Em. Heat */
    if (Envrcm_AllowedSystemSwitch[EnviracomBus][Zone] & 0x01) {
	ptr = strcat(ptr, ", ");
	ptr = strcat(ptr, Modes[0]);
    }

    /* Heat */
    if (Envrcm_AllowedSystemSwitch[EnviracomBus][Zone] & 0x02) {
	ptr = strcat(ptr, ", ");
	ptr = strcat(ptr, Modes[1]);
    }

    /* Cool */
    if (Envrcm_AllowedSystemSwitch[EnviracomBus][Zone] & 0x08) {
	ptr = strcat(ptr, ", ");
	ptr = strcat(ptr, Modes[3]);
    }

    /* Auto */
    if (Envrcm_AllowedSystemSwitch[EnviracomBus][Zone] & 0x10) {
	ptr = strcat(ptr, ", ");
	ptr = strcat(ptr, Modes[4]);
    }

    ptr[strlen(ptr)+1] = '\0';

    elog(ELOG_INFO, "Allowed Modes: Bus %d: Zone %d is 0x%x (%s)",
	    EnviracomBus, Zone, Envrcm_AllowedSystemSwitch[EnviracomBus][Zone], AllowedModes); 


}


// This function is called by
// the API whenever a device with an Enviracom clock needs to know what time it is.  The
// application should write to Envrcm_Seconds, Envrcm_Minutes, and EnvrcmHourDay, and then 
// call ChangeTime().
void App_TimeNotify(void)
{

    time_t now; 
    struct tm *tlocal;
    UNSIGNED_BYTE wday, dst;
 

    /*
     * Generate timestamp
     */
    elog(ELOG_INFO, "Time Check");

    now = time(NULL);
    tlocal = localtime(&now);
    
    /* update the Enviracom time from the application time */

    /* DST is bit 7 */
    dst = tlocal->tm_isdst << 7;

    Envrcm_Seconds = (UNSIGNED_BYTE) tlocal->tm_sec + dst;
    Envrcm_Minutes = (UNSIGNED_BYTE) tlocal->tm_min;  

    /* wday is bits 5-7 */
    wday = (UNSIGNED_BYTE) tlocal->tm_wday << 5;
    Envrcm_HourDay = (UNSIGNED_BYTE) tlocal->tm_hour + wday;

    // signal the Enviracom API to send the new time to all Enviracom clocks */
    ChangeTime();
}

// this function must be written by the application.  It is called by this API whenever 
// Envrcm_ZoneManager changes.
void App_ZoneMgrNotify(UNSIGNED_BYTE EnviracomBus)
{
    elog(ELOG_INFO, "Zone Manager: Bus %d is %d (%sPresent)", EnviracomBus,
	 Envrcm_ZoneManager[EnviracomBus], (Envrcm_ZoneManager[EnviracomBus] > 0 ? "" : "Not "));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Serial Port Functions
//////////////////////////////////////////////////////////////////////////////////////////////////

// This function must take the appropriate message in TxBuffer and configure
// so that it is sent in the background on the serial port for the Enviracom bus designated by 
// EnviracomBus.  The first byte in the message contains the number of bytes in the
// message, and this byte is not sent to the Enviracom Serial Adapter.  The bytes that follow
// must be sent in order to the serial port.  This function must not wait for the byte string
// to be sent on the serial port before returning.  The API will not call this 
// function again or overwrite the buffer until the Enviracom Serial Adapter has ACK'ed or
// NACK'ed the previous message or until a timeout period has expired, in which case the 
// application can discard the previous message. 
void App_EnvrcmSendMsg(UNSIGNED_BYTE EnviracomBus)
{

    UNSIGNED_BYTE *tptr, *ptr;

    int rtn, i, len, flags;
    time_t now;
    struct tm *tlocal;

    /* We are only listening to and dealing with Enviracom Bus == 0*/
    if (EnviracomBus > 0) return;

/*    elog(ELOG_INFO, "=> EnvrcmSendMsg: Bus: %d\n", EnviracomBus);*/


    if (cid > 0) {
/*	elog(ELOG_WARNING, "child %d still exists -- waiting", cid);*/
/*	kill(cid, SIGTERM);*/

	/*  wait for the child */
	if (cid) {
	    sigsuspend((sigset_t *) NULL);
	}

	/* hopefully catch the child now. */
    } 
	
    /* hold on Alarm and Child */
    sigprocmask(SIG_BLOCK, &bset, (sigset_t *) NULL);

    cid = 0;

    if ((cid = fork()) == 0) {

	/* We are the child */

	/* for symmetry and completeness */
	sigprocmask(SIG_UNBLOCK, &bset, (sigset_t *) NULL);

	setpgrp(0, 0);

	signal(SIGCHLD, SIG_IGN);
	signal(SIGALRM, SIG_IGN);

	tptr = (UNSIGNED_BYTE *) TxBuffer[EnviracomBus];

	len = (int) tptr[0];

	i = 1;

	/* do we have to block signals in here?  probably not as we're the child */
	if ((rtn = write(sd, (void *) &tptr[i], len)) != len) {
	    if (rtn < 0) {
		elog(ELOG_ERR, "cannot write to device: %m");
		cleanup_serial(sd);
		exit(1);
	    } else if (rtn != len) {
		elog(ELOG_ERR, "write loop error: rtn: %d len: %d\n", rtn, len);
	    } else {
		elog(ELOG_ERR, "unknown serial write error\n");
	    }
	}

	for (i = 1; i <= len; i++) {
	    if (tptr[i] == '\r') {
		tptr[i] = '\0';
	    }
	}
/*
 *	fprintf(stderr, "wrote message \"%s\" length: %d to enviracom bus: %d from pid: %d\n",
 *		&tptr[1], len, EnviracomBus, getpid());
*/


	tptr[len] = '\n';
	tptr[len+1] = '\0';

	now = time(NULL);
	tlocal = localtime(&now);

	/* write our message out the log */
	comm_log(af, tlocal, &tptr[1]);

	exit(0);

    } else if (cid == -1) {

	/* We are the error */

	/* for symmetry and completeness */
	sigprocmask(SIG_UNBLOCK, &bset, (sigset_t *) NULL);

	/* error */
	elog(ELOG_ERR, "fork failed: %m");	

	/* fall */
    } else {

	/* We are the parent */

	/* for symmetry and completeness */
	sigprocmask(SIG_UNBLOCK, &bset, (sigset_t *) NULL);

	/* we keep track of cid */

	/* fall through to ... */
    }

    return;
}

// NOTE:  The application is responsible for receiving bytes on the seral port(s) and sending
// byte strings on the serial port(s).  The serial ports must be configured for 19.2 K bits per
// second, 8 data bits, no parity, 1 stop bit. 



