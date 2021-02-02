#!/bin/sh
#
# $NetBSD: enviracomd.sh,v 1.1 2004/11/20 15:08:23 srp Exp $
#

# PROVIDE: enviracomd

$_rc_subr_loaded . /etc/rc.subr

name="enviracomd"
rcvar=$name
help_name=$name
pidfile="/var/run/${name}.pid"

command="/usr/local/etc/enviracom/${name}"
command_args="-p ${pidfile}"

load_rc_config $name
run_rc_command "$1"

