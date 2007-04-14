
MAJOR=1
MINOR=1
SUBMINOR=2

#
# Set this to your local copy of mISDN
#
MISDNDIR := /usr/src/mqueue/mISDN

PWD=$(shell pwd)
#
# Change this to create an install prefix for the shared libs, programms and
# includes
#
INSTALL_PREFIX := /
export INSTALL_PREFIX

MISDNINCLUDEDIR := $(MISDNDIR)/include
export MISDNINCLUDEDIR

mISDN_DIR := $(PWD)
export mISDN_DIR

INCLUDEDIR := $(mISDN_DIR)/include
export INCLUDEDIR

LIBDIR=/usr/lib
export LIBDIR

CFLAGS:= -g -Wall -I $(INCLUDEDIR) -I $(MISDNINCLUDEDIR)
CFLAGS+= -D CLOSE_REPORT=1

#disable this if your system does not support PIC (position independent code)
ifeq ($(shell uname -m),x86_64)
CFLAGS         += -fPIC
endif

export CFLAGS

mISDNLIB	:= $(PWD)/lib/libmISDN.a
mISDNNETLIB	:= $(PWD)/i4lnet/libmisdnnet.a
export mISDNLIB
export mISDNNETLIB

SUBDIRS := lib example

SUBDIRS += $(shell if test -d i4lnet ; then echo i4lnet; fi)
SUBDIRS += $(shell if test -d tenovis ; then echo tenovis; fi)
SUBDIRS += $(shell if test -d voip ; then echo voip; fi)
SUBDIRS += $(shell if test -d suppserv ; then echo suppserv; fi)
SUBDIRS += $(shell if test -d bridge ; then echo bridge; fi)

LIBS := lib/libmISDN.a

all: test_misdn_includes
	$(MAKE) TARGET=$@ subdirs


install_path:
	mkdir -p $(INSTALL_PREFIX)/usr/bin/
	mkdir -p $(INSTALL_PREFIX)/usr/include/mISDNuser/
	mkdir -p $(INSTALL_PREFIX)/$(LIBDIR)


install: install_path all
	$(MAKE) TARGET=install subdirs
	cp include/*.h $(INSTALL_PREFIX)/usr/include/mISDNuser/


subdirs:
	set -e; for i in $(SUBDIRS) ; do $(MAKE) -C $$i $(TARGET); done

clean:  
	$(MAKE) TARGET=$@ subdirs
	rm -f *.o *~ DEADJOE $(INCLUDEDIR)/*~ $(INCLUDEDIR)/DEADJOE

distclean: clean
	$(MAKE) TARGET=$@ subdirs
	rm -f *.o *~ testlog

MAINDIR := $(shell basename $(PWD))
ARCHIVDIR = /usr/src/packages/SOURCES
ARCHIVOPT := -v
# VERSION := $(shell date +"%Y%m%d")
VERSION := 20030423

ARCHIVNAME := $(ARCHIVDIR)/$(MAINDIR)-$(VERSION).tar.bz2

archiv: distclean
	cd ../; tar c $(ARCHIVOPT) -f - $(MAINDIR) | bzip2 > $(ARCHIVNAME)

basearchiv: ARCHIVOPT += --exclude i4lnet --exclude voip --exclude tenovis
basearchiv: ARCHIVNAME := $(ARCHIVDIR)/$(MAINDIR)_base-$(VERSION).tar.bz2
basearchiv: archiv

mainarchiv: ARCHIVOPT += --exclude voip --exclude tenovis
mainarchiv: ARCHIVNAME := $(ARCHIVDIR)/$(MAINDIR)_main-$(VERSION).tar.bz2
mainarchiv: archiv

tenovisarchiv: ARCHIVOPT += --exclude voip --exclude i4lnet
tenovisarchiv: ARCHIVNAME := $(ARCHIVDIR)/$(MAINDIR)_tenovis-$(VERSION).tar.bz2
tenovisarchiv: archiv

voiparchiv: ARCHIVOPT += --exclude tenovis
voiparchiv: ARCHIVNAME := $(ARCHIVDIR)/$(MAINDIR)_voip-$(VERSION).tar.bz2
voiparchiv: archiv


test_misdn_includes:
	@if ! echo "#include <linux/mISDNif.h>" | gcc -I$(MISDNINCLUDEDIR) -C -E - >/dev/null ; then echo -e "\n\nYou either don't seem to have installed mISDN properly\nor you haven't set the MISDNDIR variable in this very Makefile.\n\nPlease either install mISDN or set the MISDNDIR properly\n"; exit 1; fi


VERSION:
	echo $(MAJOR)_$(MINOR)_$(SUBMINOR) > VERSION

snapshot: clean
	DIR=mISDNuser-$$(date +"20%y_%m_%d") ; \
	echo $$(date +"20%y_%m_%d" | sed -e "s/\//_/g") > VERSION ; \
	mkdir -p /tmp/$$DIR ; \
	cp -a * /tmp/$$DIR ; \
	cd /tmp/; \
	tar czf $$DIR.tar.gz $$DIR

release: clean
	DIR=mISDNuser-$(MAJOR)_$(MINOR)_$(SUBMINOR) ; \
	echo $(MAJOR)_$(MINOR)_$(SUBMINOR) > VERSION ; \
	mkdir -p /tmp/$$DIR ; \
	cp -a * /tmp/$$DIR ; \
	cd /tmp/; \
	tar czf $$DIR.tar.gz $$DIR

.PHONY: VERSION clean
