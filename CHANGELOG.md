2008-11-04  Scott Presnell  <srp@tworoads.net>

	* ChangeLog: Version 0.9.3
	
	* EnviracomAPIApp.c, Makefile, enviracom.h:
	Moved VERSION into Makefile, added option to display version and build date.

	* serial.c: Standardize serial handling with other programs.

	* elog.c: Standiarize elog handling with other programs.

	* EnviracomAPI.c, EnviracomAPIApp.c, enviracom.h:
	Added tracking of "recovery" status in setpoint packets.
	Fixed resending of message when recv nack, or reset.
	Note and show which scheduled periods are not set.
	Added "OK"s after status replies.
	Closed off getpwent() with endpwent(); getgrent() with endgrent().
	Move to /var/tmp to allow for core dumps.

	* enviracomd.sh: Moved install directory to /usr/local/etc/enviracom

	* Makefile: Moved install directory to /usr/local/etc/enviracom
	Added define for extensions.

	* elog.h: New file.

