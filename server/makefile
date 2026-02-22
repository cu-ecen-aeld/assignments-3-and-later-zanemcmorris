################################################################################
# GNU makefile
# File Name: makefile
################################################################################


CC ?= ${CROSS_COMPILE}gcc
CFLAGS ?= -g -Wall -Werror
TARGET ?= aesdsocket
LDFLAGS ?= -lpthread -lrt

# Default target
aesdserver: aesdsocket.c
	$(CC) aesdsocket.c $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

.PHONY: all
all: aesdserver

.PHONY: default
all: aesdserver



clean:
	rm -f *.o *.out aesdsocket
