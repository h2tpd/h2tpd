CFLAGS = -g -Wall -O2
CFLAGS += -I../

SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

all: $(OBJECTS)

clean:
	rm -f *.o
