
EXTRA_CFLAGS :=
EXTRA_INCLUDE :=
EXTRA_LIB :=

PROGRAMMS = isdninfo

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

INFOOBJ = info.o

isdninfo: $(INFOOBJ)
	$(CC) $(INFOOBJ) $(LFLAGS) -o $@

info.o: info.c

clean:
	rm -f *.o DEADJOE
	find ./ -name '*~' -exec rm {} \;
	rm -f $(PROGRAMMS)

distclean: clean
	rm -f *.a $(PROGRAMMS)
