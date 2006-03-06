
all: libmISDN.a libmISDN.so

install:
	install -m 644 libmISDN.so $(INSTALL_PREFIX)/usr/lib


LIBMISDN_OBJS = device.o layer.o stack.o status.o

libmISDN.so: $(LIBMISDN_OBJS)
	rm -f $@
	$(CC) -shared -Xlinker -x -o $@ $(LIBMISDN_OBJS)


libmISDN.a: $(LIBMISDN_OBJS)
	rm -f $@
	ar -r $@ $^
	ar -s $@

device.o : device.c ../include/mISDNlib.h
layer.o : layer.c ../include/mISDNlib.h
stack.o : stack.c ../include/mISDNlib.h
status.o : status.c ../include/mISDNlib.h 

clean:
	rm -f *.o *~ DEADJOE
	rm -f libmISDN.a

distclean: clean
	rm -f *.a
 
