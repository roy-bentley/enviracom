/*
 *
 * Author: Scott R. Presnell
 *
 * Copyright (C) 2008
 */

/*
 * elog.h - elogging defines and delcarations
 *
 * Scott Presnell, srp@tworoads.net, Thu Aug 21 16:29:04 PDT 2008
 *
 */
extern int switch_to_logfile (char *);
extern void elog(int lvl, char *fmt, ...);

#undef HAVE_H_ERRLIST

/*
 * Logging level flags
 */
#define ELOG_FATAL      0000001 /* critical conditions */
#define ELOG_ERR        0000002 /* error conditions */
#define ELOG_WARNING    0000004 /* warning conditions */
#define ELOG_USER       0000010 /* debug-level messages */
#define ELOG_INFO       0000020 /* informational */
#define ELOG_STATS      0000040 /* normal but significant condition */
#define ELOG_TRACE      0000100 /* trace protocol messages */
#define ELOG_DEBUG      0000200 /* debugging messages */
#define ELOG_ALL        0000177 /* All error messages */
