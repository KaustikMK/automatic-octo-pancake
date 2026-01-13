NAME = SMBEMU

TARGET ?= ti84pce
COMPRESSED ?= YES
ARCHIVED ?= YES

CFLAGS = -Wall -Wextra -O0 -std=c99
LTO = NO

CEDEV ?= $(shell cedev-config --prefix 2>/dev/null)
ifeq ($(CEDEV),)
$(error CEDEV not set and cedev-config not found in PATH)
endif

include $(CEDEV)/meta/makefile.mk
