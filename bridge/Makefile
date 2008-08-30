# ifndef SF_DIR
# SF_DIR = /home/kkeil/speak_freely-7.2
# endif

EXTRA_CFLAGS :=
EXTRA_INCLUDE :=
EXTRA_LIB :=

PROGRAMMS = isdnbridge

all: $(PROGRAMMS)

install:
	for i in $(PROGRAMMS) ; do \
		install -m 755 $$i $(INSTALL_PREFIX)/usr/bin ;\
	done

CARGS	=

CCFLAGS	= -Wall -DLINUX -DM_LITTLE_ENDIAN

LFLAGS	= 

DEBUG	= 

CFLAGS	:= $(CFLAGS) $(DEBUG) $(EXTRA_INCLUDE) \
	$(CARGS) $(DUPLEX) $(CCFLAGS) $(DOMAIN) $(EXTRA_CFLAGS)

BRIDGEOBJ = bridge.o

isdnbridge: $(BRIDGEOBJ)
	$(CC) $(BRIDGEOBJ) $(LFLAGS) -o $@

bridge.o: bridge.c ../include/mlayer3.h ../include/compat_af_isdn.h

clean:
	rm -f *.o DEADJOE
	find ./ -name '*~' -exec rm {} \;
	rm -f $(PROGRAMMS)

distclean: clean
	rm -f *.a $(PROGRAMMS)
