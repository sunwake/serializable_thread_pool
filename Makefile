.SUFFIXES: .o .C .cxx .c

LIB_PATH=-L/usr/lib
LIB_DIR = .
INCDIR = -I/usr/include

CC	= `g++`
CCFLAGS = `-fpermissive -c -g  -D_LINUX -pthread -fPIC -Wall -Wno-format-y2k -D_LITTLEENDIAN `


.c.o:
	@echo	$(CC) $(CCFLAGS) $(INCDIR) $< 
	@	$(CC) $(CCFLAGS) $(INCDIR) $<

all : test
test:
	g++ threadpool.o threadpool_test.o -o test -pthread
clean:
	rm -f *.o test

.PHONY: all clean scp
