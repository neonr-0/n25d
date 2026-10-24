CC=gcc
INCLUDE=-I./headers -I./depends
CFLAGS= $(INCLUDE)
AR=ar
LDFLAGS=
SOURCES = n25d.c n25dfile.c
TARGET=builds/n25d.a

OBJECTS = $(addprefix src/,$(SOURCES:.c=.o))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	mkdir -p builds
	$(AR) crv $@ $^

#library
libn25d: $(OBJECTS)
	$(AR) crv $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

#Model editor
#n25dmodeleditor : n25dgl.o kbd.o command.o display.o \
#       insert.o search.o files.o utils.o
#        cc -o edit main.o kbd.o command.o display.o \
#                   insert.o search.o files.o utils.o

n25dgl.o : n25dgl.c $(CFLAGS)
	$(CC) -o n25dgl.o

utils.o : utils.c defs.h
	cc -c utils.c

.PHONY: test test-odr test-all clean

#Tests
test:
	@fails=0; \
	for t in $(filter-out test/odr_%,$(wildcard test/*.c)); do \
		bin=$$(basename $$t .c); \
		if $(CC) $(CFLAGS) -o /tmp/$$bin $$t src/n25d.c src/n25dfile.c -lm 2> /tmp/$$bin.err; then \
			if /tmp/$$bin > /tmp/$$bin.out 2> /tmp/$$bin.run.err; then \
				echo "PASS $$bin"; \
			else \
				echo "FAIL $$bin"; cat /tmp/$$bin.out /tmp/$$bin.run.err; fails=1; \
			fi; \
		else \
			echo "COMPILE FAIL $$bin"; cat /tmp/$$bin.err; fails=1; \
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
