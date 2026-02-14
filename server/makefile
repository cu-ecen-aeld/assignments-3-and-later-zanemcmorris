################################################################################
# GNU makefile
# File Name: makefile
################################################################################


CC := gcc 

ifeq ($(CROSS_COMPILE),)
TARGET := aesdsocket
else
TARGET := aesdsocket
endif

ifneq ($(CROSS_COMPILE),)
CC := $(CROSS_COMPILE)gcc
endif

# Default target
aesdserver: aesdsocket.c
	$(CC) aesdsocket.c $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

.PHONY: all
all: aesdserver

.PHONY: default
all: aesdserver



clean:
	rm -f *.o *.out aesdsocket
