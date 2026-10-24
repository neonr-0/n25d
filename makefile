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

.PHONY: test test-odr test-all

#Tests
test:
	@fails=0; \
	for t in $(filter-out test/odr_%,$(wildcard test/*.c)); do \
		bin=$$(basename $$t .c); \
		if $(CC) $(CFLAGS) -o /tmp/$$bin $$t src/n25d.c src/n25dfile.c -lm 2>/dev/null; then \
			if /tmp/$$bin > /tmp/$$bin.out 2>&1; then \
				echo "PASS $$bin"; \
			else \
				echo "FAIL $$bin"; cat /tmp/$$bin.out; fails=1; \
			fi; \
		else \
			echo "COMPILE FAIL $$bin"; fails=1; \
		fi; \
	done; \
	exit $$fails

#ODR check: byte_order.h must be includable from many translation units
test-odr:
	$(CC) $(CFLAGS) -c test/odr_a.c -o /tmp/odr_a.o
	$(CC) $(CFLAGS) -c test/odr_b.c -o /tmp/odr_b.o
	$(CC) $(CFLAGS) -o /tmp/odr_link /tmp/odr_a.o /tmp/odr_b.o test/odr_main.c src/n25d.c src/n25dfile.c -lm
	/tmp/odr_link

test-all: test test-odr

clean:
	rm -rf $(TARGET) $(OBJECTS)
 