################################################################################
# GNU makefile
# File Name: makefile
################################################################################


CC := gcc 

ifeq ($(CROSS_COMPILE),)
TARGET := writer
else
TARGET := writer
endif

ifneq ($(CROSS_COMPILE),)
CC := $(CROSS_COMPILE)gcc
endif

# Default target
writer: writer.c
# 	$(CC) writer.c -o $(TARGET)
	$(CC) writer.c $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

.PHONY: all
all: writer


clean:
	rm -f *.o *.out writer
