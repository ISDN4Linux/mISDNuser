
all: libisdnnet.a libisdnnet.so

install:
	install -m 644 libisdnnet.so $(INSTALL_PREFIX)/usr/lib
	cp *.h $(INSTALL_PREFIX)/usr/include/mISDNuser/

ISDNNETOBJ = net_if.o isdn_debug.o isdn_msg.o fsm.o net_l2.o tei.o net_l3.o \
		manager.o tone.o bchannel.o g711.o

libisdnnet.so: $(ISDNNETOBJ)
	rm -f $@
	$(CC) -shared -Xlinker -x -o $@ $(ISDNNETOBJ)

libisdnnet.a: $(ISDNNETOBJ)
	rm -f $@
	ar cr $@ $(ISDNNETOBJ)

isdn_msg.o : isdn_msg.c $(INCLUDEDIR)/isdn_msg.h $(INCLUDEDIR)/isdn_net.h

isdn_debug.o : isdn_debug.c $(INCLUDEDIR)/isdn_debug.h

net_l2.o : net_l2.c net_l2.h $(INCLUDEDIR)/isdn_net.h fsm.h

fsm.o : fsm.c fsm.h $(INCLUDEDIR)/isdn_net.h

tei.o : tei.c net_l2.h $(INCLUDEDIR)/isdn_net.h

net_l3.o : net_l3.c $(INCLUDEDIR)/isdn_net.h net_l3.h

manager.o : manager.c $(INCLUDEDIR)/isdn_net.h $(INCLUDEDIR)/bchannel.h

net_if.o : net_if.c $(INCLUDEDIR)/isdn_net.h

tone.o: tone.c $(INCLUDEDIR)/tone.h $(INCLUDEDIR)/bchannel.h \
	$(INCLUDEDIR)/isdn_net.h $(INCLUDEDIR)/ibuffer.h

bchannel.o: bchannel.c $(INCLUDEDIR)/isdn_net.h $(INCLUDEDIR)/tone.h \
	$(INCLUDEDIR)/bchannel.h net_l3.h $(INCLUDEDIR)/ibuffer.h

g711.o: g711.c $(INCLUDEDIR)/g711.h

clean:
	rm -f *.o *~ DEADJOE
	rm -f libisdnnet.a

distclean: clean
	rm -f *.a
