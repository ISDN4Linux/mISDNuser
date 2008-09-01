
EXTRA_CFLAGS :=
EXTRA_INCLUDE :=
EXTRA_LIB :=

PROGRAMS = misdninfo misdnrename

all: $(PROGRAMS)

install:
	for i in $(PROGRAMS) ; do \
		install -m 755 $$i $(INSTALL_PREFIX)/usr/bin ;\
	done

CARGS	=

CCFLAGS	= -Wall -DLINUX -DM_LITTLE_ENDIAN

LFLAGS	= 

DEBUG	= 

CFLAGS	:= $(CFLAGS) $(DEBUG) $(EXTRA_INCLUDE) \
	$(CARGS) $(DUPLEX) $(CCFLAGS) $(DOMAIN) $(EXTRA_CFLAGS)

INFOOBJ = info.o
NAMEOBJ = rename.o

misdninfo: $(INFOOBJ)
	$(CC) $(INFOOBJ) $(LFLAGS) -o $@

misdnrename: $(NAMEOBJ)
	$(CC) $(NAMEOBJ) $(LFLAGS) -o $@

info.o: info.c ../include/mlayer3.h ../include/compat_af_isdn.h
rename.o: rename.c ../include/mISDNif.h ../include/mlayer3.h ../include/compat_af_isdn.h

clean:
	rm -f *.o DEADJOE
	find ./ -name '*~' -exec rm {} \;
	rm -f $(PROGRAMS)

distclean: clean
	rm -f *.a $(PROGRAMS)
