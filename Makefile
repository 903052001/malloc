#
# Copyright (C) Lance. 2023. All rights reserved
# Use 'make V=1' to see the full compile commands
#

SHELL   := /bin/sh
TARGET  := priv_malloc

CURDIR  := $(shell pwd)
$(shell if [ ! -d "output" ]; then mkdir output; fi)
OBJPATH := $(CURDIR)/output

ifeq ("$(origin V)", "command line")
    KBUILD_VERBOSE = $(V)
else
    KBUILD_VERBOSE = 0
endif

ifeq ($(KBUILD_VERBOSE),1)
    Q =
    ifndef VERBOSE
    VERBOSE = 1
    endif
    export VERBOSE
else
    Q = @
endif

CROSS_COMPILE :=
export AS   = $(CROSS_COMPILE)as
export LD   = $(CROSS_COMPILE)ld
export CC   = $(CROSS_COMPILE)gcc
export BIN  = $(CROSS_COMPILE)objcopy -O binary
export HEX  = $(CROSS_COMPILE)objcopy -O ihex
export DUMP = $(CROSS_COMPILE)objdump -h -t
export SIZE = $(CROSS_COMPILE)size
export GDB  = $(CROSS_COMPILE)gdb
export A2L  = $(CROSS_COMPILE)addr2line

CCFLAGS  := -g -O0 -Wall -std=c89
LDFLAGS  := -Map $(OBJPATH)/$(TARGET).map

ALL_SRCS += priv_malloc.c
ALL_OBJS := $(ALL_SRCS:%.c=%.o)


all : $(ALL_SRCS)
	$(Q) $(CC)  $(CCFLAGS) $^ -o $(OBJPATH)/$(TARGET).elf
	$(Q) $(HEX) $(OBJPATH)/$(TARGET).elf  $(OBJPATH)/$(TARGET).hex

clean :
	$(Q) $(RM) -rf $(OBJPATH)
