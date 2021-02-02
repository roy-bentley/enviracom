#
# Enviracom Application Makefile.
# Scott Presnell, srp@tworoads.net, Tue Jul  8 09:32:53 PDT 2008
#
VERSION=0.9.3
BUILD_DATE=$(shell date +%F)

RCDIR=/etc/rc.d
BINDIR=/usr/local/etc/enviracom

CFLAGS=-g -O -DDEBUG -DEXTENSIONS -DVERSION=\"$(VERSION)\" -DBUILD_DATE=\"$(BUILD_DATE)\"

LDFLAGS=$(CFLAGS)



all: enviracomd

enviracomd: EnviracomAPIApp.o EnviracomAPI.o serial.o elog.o
	$(CC) -o enviracomd EnviracomAPIApp.o EnviracomAPI.o serial.o elog.o

install: install.bin install.rc

install.bin: $(BINDIR)/enviracomd

$(BINDIR)/enviracomd: enviracomd
	install -m 755 $? $(BINDIR)/$?

install.rc: $(RCDIR)/enviracomd

$(RCDIR)/enviracomd: enviracomd.sh
	install -m 755 $? $(RCDIR)/enviracomd

tarball: clobber
	cd .. && tar -cvzf enviracom-$(VERSION).tar.gz -T enviracom/.tarball

clean:
	rm -f *.o

clobber: clean
	rm -f enviracomd enviracomd.core
