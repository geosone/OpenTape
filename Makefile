OPEN386MAX ?= ../386MAX
OPENASPI   ?= ../OpenAspi
GDI        ?= ../gdi
CC         ?= cc

BUILD   := build
DOSBIN  := $(BUILD)/dos/OPENTAPE.EXE
HOSTBIN := $(BUILD)/host/test_backend
DISTDIR := $(BUILD)/dist

CFLAGS  ?= -std=c90 -Wall -Wextra -Werror -pedantic -O2
CPPFLAGS += -Iinclude
CPPFLAGS += -I$(OPENASPI)/include

.PHONY: all test check dist clean

all: $(DOSBIN) $(HOSTBIN)

test: $(HOSTBIN)
	$(HOSTBIN)

check: all test

$(DOSBIN): src/main.c core/backend.c include/opentape/backend.h dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) \
		$(abspath $@)

$(HOSTBIN): tests/test_backend.c core/backend.c include/opentape/backend.h \
		$(OPENASPI)/include/openaspi/aspitape.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_backend.c core/backend.c

dist: all $(GDI)/build/INSTALL.EXE LICENSE
	rm -rf $(DISTDIR)
	mkdir -p $(DISTDIR)
	cp $(DOSBIN) $(GDI)/build/INSTALL.EXE installer/INSTALL.INI \
		installer/README.TXT LICENSE $(DISTDIR)/
	mv $(DISTDIR)/LICENSE $(DISTDIR)/LICENSE.TXT

$(GDI)/build/INSTALL.EXE:
	$(MAKE) -C $(GDI)

clean:
	rm -rf $(BUILD)
