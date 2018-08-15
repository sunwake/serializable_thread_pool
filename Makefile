.SUFFIXES: .o .C .cxx .c

OBJECTS1 = threadpool.o
TARGET   = nothing

LIB_PATH=-L/usr/lib
#LIB_DIR = $(SCADA_HOME)/lib
LIB_DIR = .
INCDIR = -I/usr/include

CC	= `makeopts.sh cc`
CCFLAGS = `makeopts.sh ccflag`


LIBS	= `makeopts.sh socket` -lpthread
MK_SO = `makeopts.sh mk_so`
LDFLAGS = `makeopts.sh ldflag`

.c.o:
	@echo	$(CC) $(CCFLAGS) $(INCDIR) $< 
	@	$(CC) $(CCFLAGS) $(INCDIR) $<

all : $(TARGET)

$(TARGET):$(OBJECTS1)
	cp -rp $(LIB_DIR)/$(OBJECTS1) ../replicate

install:
	cp -rp $(LIB_DIR)/$(OBJECTS1) ../replicate
test:
	g++ threadpool.o threadpool_test.o -o test -pthread
clean:
	rm -f *.o

.PHONY: all clean scp
