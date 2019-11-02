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



clean:
	rm -rf $(TARGET) $(OBJECTS) 