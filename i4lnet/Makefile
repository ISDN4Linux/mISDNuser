CC = gcc
AR = ar
RANLIB = ranlib

all: libisdnnet.a libisdnnet_pic.a libisdnnet.so

install:
	install -m 644 libisdnnet.a $(INSTALL_PREFIX)/usr/lib
	install -m 644 libisdnnet_pic.a $(INSTALL_PREFIX)/usr/lib
	install -m 644 libisdnnet.so $(INSTALL_PREFIX)/usr/lib
	cp *.h $(INSTALL_PREFIX)/usr/include/mISDNuser/

ISDNNET_OBJ = net_if.o isdn_debug.o isdn_msg.o fsm.o net_l2.o tei.o net_l3.o \
		manager.o tone.o bchannel.o g711.o
ISDNNET_PICOBJ = $(ISDNNET_OBJ:%.o=%.lo)

ifeq ($(shell uname -m),x86_64)
CFLAGS         += -fPIC
endif


libisdnnet_pic.a: $(ISDNNET_PICOBJ)
	$(AR) cru $@ $^
	$(RANLIB) $@

libisdnnet.a: $(ISDNNET_OBJ)
	$(AR) cru $@ $^
	$(RANLIB) $@

libisdnnet.so: $(ISDNNET_OBJ)
	$(CC) $(CFLAGS) -shared -Xlinker -x -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

.c.lo:
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

isdn_msg.o isdn_msg.lo: isdn_msg.c $(INCLUDEDIR)/isdn_msg.h $(INCLUDEDIR)/isdn_net.h

isdn_debug.o isdn_debug.lo: isdn_debug.c $(INCLUDEDIR)/isdn_debug.h

net_l2.o net_l2.lo: net_l2.c net_l2.h $(INCLUDEDIR)/isdn_net.h fsm.h

fsm.o fsm.lo: fsm.c fsm.h $(INCLUDEDIR)/isdn_net.h

tei.o tei.lo: tei.c net_l2.h $(INCLUDEDIR)/isdn_net.h

net_l3.o net_l3.lo: net_l3.c $(INCLUDEDIR)/isdn_net.h net_l3.h

manager.o manager.lo: manager.c $(INCLUDEDIR)/isdn_net.h $(INCLUDEDIR)/bchannel.h

net_if.o net_if.lo: net_if.c $(INCLUDEDIR)/isdn_net.h

tone.o tone.lo: tone.c $(INCLUDEDIR)/tone.h $(INCLUDEDIR)/bchannel.h \
	$(INCLUDEDIR)/isdn_net.h $(INCLUDEDIR)/ibuffer.h

bchannel.o bchannel.lo: bchannel.c $(INCLUDEDIR)/isdn_net.h $(INCLUDEDIR)/tone.h \
	$(INCLUDEDIR)/bchannel.h net_l3.h $(INCLUDEDIR)/ibuffer.h

g711.o g711.lo: g711.c $(INCLUDEDIR)/g711.h

clean:
	rm -f *.o *.lo *~ DEADJOE 
	rm -f libisdnnet.a libisdnnet_pic.a libisdnnet.so

distclean: clean
	rm -f *.a

.SUFFIXES: .lo
