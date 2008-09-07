
EXTRA_CFLAGS :=
EXTRA_INCLUDE :=
EXTRA_LIB :=

PROGRAMS = misdn_info misdn_rename misdn_cleanl2 misdn_log

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
CLEANOBJ = cleanl2.o
LOGOBJ = loghex.o

misdn_info: $(INFOOBJ)
	$(CC) $(INFOOBJ) $(LFLAGS) -o $@

misdn_rename: $(NAMEOBJ)
	$(CC) $(NAMEOBJ) $(LFLAGS) -o $@

misdn_cleanl2: $(CLEANOBJ)
	$(CC) $(CLEANOBJ) $(LFLAGS) -o $@

misdn_log: $(LOGOBJ)
	$(CC) $(LOGOBJ) $(LFLAGS) -o $@

info.o: info.c ../include/mlayer3.h ../include/compat_af_isdn.h
rename.o: rename.c ../include/mISDNif.h ../include/mlayer3.h ../include/compat_af_isdn.h
cleanl2.o: cleanl2.c ../include/mISDNif.h ../include/compat_af_isdn.h
loghex.o: loghex.c ../include/mISDNif.h  ../include/compat_af_isdn.h


clean:
	rm -f *.o DEADJOE
	find ./ -name '*~' -exec rm {} \;

distclean: clean
	rm -f *.a $(PROGRAMS)
