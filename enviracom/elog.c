#ifndef lint
static char rcsid[] = "$Id: elog.c,v 1.2 2008/11/04 17:28:15 srp Exp srp $";
#endif

/*
 *
 * Author: Scott R. Presnell
 *
 * Copyright (C) 2008
 */

/*
 * elog.c - various routines for manipulating and writing the log files,
 *		and other information on the queues or the daemon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "elog.h"

extern char *	progname;

FILE *		logfp = stderr;
FILE *		sfp = stderr;			/* Stats logging descriptor */
static int	syslogging = 0;

int		switch_to_logfile (char *);
void		elog(int lvl, char *fmt, ...);

static void	fakelog (int);
static void	expand_error(char *f, char *e);

/*
 * Report a serious error.  May call abort of debugging a fatal error.
 */
void
elog(int lvl, char *fmt, ...)

{
	va_list	args;
	char	priv[BUFSIZ];
	char	nfmt[BUFSIZ];
	char *	ptr;
	int	level;		/* real syslog level */	


	ptr = priv;

	/* Fire up the variable argument mechanism */
	/*CONSTCOND*/
	va_start(args, fmt);

	switch (lvl) {
	case ELOG_FATAL:
		level = LOG_CRIT; break;
	case ELOG_ERR:
		level = LOG_ERR; break;
	case ELOG_WARNING:
		level = LOG_WARNING; break;
	case ELOG_USER:
		level = LOG_WARNING; break;
	case ELOG_INFO:
		level = LOG_INFO; break;
	case ELOG_STATS:
		level = LOG_INFO; break;
	case ELOG_TRACE:
		level = LOG_DEBUG; break;
	case ELOG_DEBUG:
		level = LOG_DEBUG; break;
	default:
		level = LOG_ERR; break;
	}

	expand_error(fmt, nfmt);

	(void) vsprintf(priv, nfmt, args);

	ptr += strlen(ptr);
	
	if (syslogging) {
		if (ptr[-1] == '\n')
			*--ptr  = '\0';

		syslog(level, priv);
	} else {
		if (ptr[-1] != '\n') {
			*ptr++ = '\n';
			*ptr = '\0';
		}

		fakelog(lvl);
		(void) fwrite(priv, ptr - priv, 1, logfp);
		(void) fflush(logfp);
	}
	
	/* Shut down the variable argument mechanism */
	va_end(args);

	return ;

}



/*
 * Fake a syslog style time of day and hostname to the logfile
 */
static void
fakelog(lvl)
int	lvl;
{

	char *	sev;
	time_t	t = time((time_t *) 0);
	static char	last_ctime[BUFSIZ];
	static time_t	last_t = 0;
	static int 	my_pid;

	my_pid = getpid();


	if (t != last_t) {
		(void) strcpy(last_ctime, ctime(&t));
		last_t = t;
	}

	switch (lvl) {
	case ELOG_FATAL:
		sev = "fatal:"; break;
	case ELOG_ERR:
 		sev = "error:"; break;
	case ELOG_USER:
		sev = "user: "; break;
	case ELOG_WARNING:
		sev = "warn: "; break;
	case ELOG_INFO:
		sev = "info: "; break;
	case ELOG_STATS:
		sev = "stats:"; break;
	case ELOG_TRACE:
		sev = "trace:"; break;
	case ELOG_DEBUG:
		sev = "debug:"; break;
	default:
		sev = "unk:  "; break;
	}

	(void) fprintf(logfp, "%15.15s %s %s[%d]/%s ",
		       last_ctime+4, "localhost", progname, my_pid, sev);
}


/*
 * get syslog facility to use.
 * logfile can be "syslog", "syslog:daemon", "syslog:local7", etc.
 */
static int
get_syslog_facility(const char *logfile)
{
  char *facstr;

  /* parse facility string */
  facstr = strchr(logfile, ':');
  if (!facstr)			/* log file was "syslog" */
    return LOG_DAEMON;
  facstr++;
  if (!facstr || facstr[0] == '\0') { /* log file was "syslog:" */
    return LOG_DAEMON;
  }

#ifdef LOG_KERN
  if (!strcmp(facstr, "kern"))
      return LOG_KERN;
#endif /* not LOG_KERN */
#ifdef LOG_USER
  if (!strcmp(facstr, "user"))
      return LOG_USER;
#endif /* not LOG_USER */
#ifdef LOG_MAIL
  if (!strcmp(facstr, "mail"))
      return LOG_MAIL;
#endif /* not LOG_MAIL */

  if (!strcmp(facstr, "daemon"))
      return LOG_DAEMON;

#ifdef LOG_AUTH
  if (!strcmp(facstr, "auth"))
      return LOG_AUTH;
#endif /* not LOG_AUTH */
#ifdef LOG_SYSLOG
  if (!strcmp(facstr, "syslog"))
      return LOG_SYSLOG;
#endif /* not LOG_SYSLOG */
#ifdef LOG_LPR
  if (!strcmp(facstr, "lpr"))
      return LOG_LPR;
#endif /* not LOG_LPR */
#ifdef LOG_NEWS
  if (!strcmp(facstr, "news"))
      return LOG_NEWS;
#endif /* not LOG_NEWS */
#ifdef LOG_UUCP
  if (!strcmp(facstr, "uucp"))
      return LOG_UUCP;
#endif /* not LOG_UUCP */
#ifdef LOG_CRON
  if (!strcmp(facstr, "cron"))
      return LOG_CRON;
#endif /* not LOG_CRON */
#ifdef LOG_LOCAL0
  if (!strcmp(facstr, "local0"))
      return LOG_LOCAL0;
#endif /* not LOG_LOCAL0 */
#ifdef LOG_LOCAL1
  if (!strcmp(facstr, "local1"))
      return LOG_LOCAL1;
#endif /* not LOG_LOCAL1 */
#ifdef LOG_LOCAL2
  if (!strcmp(facstr, "local2"))
      return LOG_LOCAL2;
#endif /* not LOG_LOCAL2 */
#ifdef LOG_LOCAL3
  if (!strcmp(facstr, "local3"))
      return LOG_LOCAL3;
#endif /* not LOG_LOCAL3 */
#ifdef LOG_LOCAL4
  if (!strcmp(facstr, "local4"))
      return LOG_LOCAL4;
#endif /* not LOG_LOCAL4 */
#ifdef LOG_LOCAL5
  if (!strcmp(facstr, "local5"))
      return LOG_LOCAL5;
#endif /* not LOG_LOCAL5 */
#ifdef LOG_LOCAL6
  if (!strcmp(facstr, "local6"))
      return LOG_LOCAL6;
#endif /* not LOG_LOCAL6 */
#ifdef LOG_LOCAL7
  if (!strcmp(facstr, "local7"))
      return LOG_LOCAL7;
#endif /* not LOG_LOCAL7 */

  return LOG_DAEMON;
}

/*
 * Change the current logfile
 */
int
switch_to_logfile(newlogfile)
char *	newlogfile;
{

	FILE *	new_logfp = stderr;


	if (newlogfile) {
#if 0
	        fprintf(stderr, "newlog: %s, newfacility: %d\n", newlogfile, get_syslog_facility(newlogfile));
#endif
		syslogging = 0;
		if (strcmp(newlogfile, "/dev/stderr") == 0) {
			new_logfp = stderr;
		} else if (strcmp(newlogfile, "stderr") == 0) {
			new_logfp = stderr;
		} else if (strncmp(newlogfile, "syslog", 6) == 0) {
			syslogging = 1;
			new_logfp = stderr;

			/*
			 * Existence of LOG_DAEMON indicates a three
			 * parameter openlog()
			 */
			openlog(progname, LOG_PID|LOG_NOWAIT
# ifdef LOG_DAEMON
				, get_syslog_facility(newlogfile)
#endif /* LOG_DAEMON */
				);

		} else {
			new_logfp = fopen(newlogfile, "a");
		}
	}

	/*
	 * If we couldn't open a new file, then continue using the old
	 */
	if (!new_logfp && newlogfile) {
		elog(ELOG_USER, "%s: Can't open logfile: %m", newlogfile);
		return (-1);
	}

	/*
	 * Close the previous file
	 */
	if (logfp && logfp != stderr)
		(void) fclose(logfp);
	logfp = new_logfp;
	return 0;
}

#if 0
#ifdef HAVE_H_ERRLIST

extern int	h_nerr;
extern char *	h_errlist[];

#else

/*
 * Table of hostname lookup error strings
 */
char    *h_errlist[] = {
        "Error 0",				/* 0 ERROR_NOT_FOUND */
        "Unknown host",                         /* 1 HOST_NOT_FOUND */
        "Host name lookup failure",             /* 2 TRY_AGAIN */
        "Unknown server error",                 /* 3 NO_RECOVERY */
        "No address associated with name",      /* 4 NO_ADDRESS */
};
int     h_nerr = { sizeof(h_errlist)/sizeof(h_errlist[0]) };

#endif	/* HAVE_H_ERRLIST */
#endif

/*
 * Take a syslog format string and expand occurences of special keys.
 *
 * %m:	expand with string from current errno (system call errors)
 * %h:	expand with string from current h_errno (DNS call errors)
 * (not implemented yet:)
 * %b:	expand with string from current b_errno (batch errors)
 */
void
expand_error(f, e)
char *	f;
char *	e;
{

	char *	p;
	int	error = errno;

	for (p = f; *e = *p; e++, p++) {
		if (p[0] == '%' && p[1] == 'm') {
			const char *errstr;
			if (error < 0 || error >= sys_nerr)
				errstr = 0;
			else
				errstr = sys_errlist[error];
			if (errstr)
				(void) strcpy(e, errstr);
			else
				(void) sprintf(e, "Error %d", error);
			e += strlen(e) - 1;
			p++;
		}
#if 0
		if (p[0] == '%' && p[1] == 'h') {
			const char *errstr;
			if (h_errno < 0 || h_errno >= h_nerr)
				errstr = 0;
			else
				errstr = h_errlist[h_errno];
			if (errstr)
				(void) strcpy(e, errstr);
			else
				(void) sprintf(e, "Host error %d", h_errno);
			e += strlen(e) - 1;
			p++;
		}
#endif
	}
}
