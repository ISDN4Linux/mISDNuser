
#CFLAGS += -DASN1_DEBUG

SUPPSERVOBJ = asn1.o asn1_enc.o asn1_generic.o asn1_aoc.o asn1_basic_service.o asn1_comp.o asn1_diversion.o asn1_address.o fac.o

all: libsuppserv.a libsuppserv.so

main: $(SUPPSERVOBJ)

install:
	install -m 644 libsuppserv.so $(INSTALL_PREFIX)/usr/lib
	install -m 644 libsuppserv.a $(INSTALL_PREFIX)/usr/lib
	cp *.h $(INSTALL_PREFIX)/usr/include/mISDNuser/

libsuppserv.so: $(SUPPSERVOBJ)
	$(CC) -shared -Xlinker -x -o $@ $^

libsuppserv.a: $(SUPPSERVOBJ)
	ar cr $@ $^
	ar s $@

clean:
	rm -f *.o *~ DEADJOE
	rm -f libsuppserv.a libsuppserv.so

distclean: clean
	rm -f *.a
