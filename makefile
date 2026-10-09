CC=gcc
INCLUDE=-I./headers -I./depends
CFLAGS= $(INCLUDE)
AR=ar
LDFLAGS=
SOURCES = *.c
TARGET=builds/n25d.a


OBJECTS = $(shell echo src/$(SOURCES) | sed -e 's,\.c,\.o,g')
#OBJECTS=$(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS) 
	$(AR) crv $@ $^
#library

libn25d: n25d.o n25dfile.o
	$(AR) crv $@ $^

n25d.o: src/n25d.c $(CFLAGS)
	$(CC) -o n25d.o
n25dfile.o: src/n25dfile.c $(CFLAGS)
	$(CC) -o n25dfile.o

#Model editor
#n25dmodeleditor : n25dgl.o kbd.o command.o display.o \
#       insert.o search.o files.o utils.o
#        cc -o edit main.o kbd.o command.o display.o \
#                   insert.o search.o files.o utils.o

n25dgl.o : n25dgl.c $(CFLAGS)
	$(CC) -o n25dgl.o


utils.o : utils.c defs.h
	cc -c utils.c

clean:
	rm -rf $(TARGET) $(OBJECTS) 