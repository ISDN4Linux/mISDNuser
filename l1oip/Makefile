
EXTRA_CFLAGS :=
EXTRA_INCLUDE :=
EXTRA_LIB :=

PROGRAMMS = l1oipctrl

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

L1OIPOBJ = l1oipctrl.o

l1oipctrl: $(L1OIPOBJ)
	$(CC) $(L1OIPOBJ) $(LFLAGS) -o $@

l1oipctrl.o: l1oipctrl.c

clean:
	rm -f *.o DEADJOE
	find ./ -name '*~' -exec rm {} \;
	rm -f $(PROGRAMMS)

distclean: clean
	rm -f *.a $(PROGRAMMS)
