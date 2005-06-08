
all: libmISDN.a

libmISDN.a: device.o layer.o stack.o status.o
	rm -f $@
	ar -r $@ $^
	ar -s $@

device.o : device.c ../include/mISDNlib.h
layer.o : layer.c ../include/mISDNlib.h
stack.o : stack.c ../include/mISDNlib.h
status.o : status.c ../include/mISDNlib.h 

clean:
	rm -f *.o *~ DEADJOE

distclean: clean
	rm -f *.a
 