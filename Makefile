
MAJOR=1
MINOR=1
SUBMINOR=1

PWD=$(shell pwd)
#
# Change this to create an install prefix for the shared libs, programms and
# includes
#
INSTALL_PREFIX := /
export INSTALL_PREFIX

mISDN_DIR := $(PWD)
export mISDN_DIR

INCLUDEDIR := $(mISDN_DIR)/include
export INCLUDEDIR

LIBDIR := $(mISDN_DIR)/layer3
export LIBDIR

CFLAGS:= -g -Wall -I $(INCLUDEDIR)

LDFLAGS:= -g -L $(LIBDIR)

#disable this if your system does not support PIC (position independent code)
ifeq ($(shell uname -m),x86_64)
CFLAGS         += -fPIC
endif

export CFLAGS

SUBDIRS := layer3 bridge info example


all:
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
