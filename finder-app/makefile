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
	$(CC) writer.c -o $(TARGET)

clean:
	rm -f *.o *.out writer
