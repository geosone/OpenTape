OPEN386MAX ?= deps/Open386MAX
OPENASPI   ?= deps/OpenASPI
GDI        ?= deps/gdi
WXWINDOWS  ?= deps/wxWindows
WATCOM     ?= /opt/watcom
CC         ?= cc
MAXLANG    ?= US

SUPPORTED_LANGS := US GR
ifeq ($(filter $(MAXLANG),$(SUPPORTED_LANGS)),)
$(error Unsupported MAXLANG '$(MAXLANG)'; use US or GR)
endif

BUILD   ?= build/$(MAXLANG)
DOSBIN  := $(BUILD)/dos/OPENTAPE.EXE
WINBIN  := $(BUILD)/win16/OTAPEWIN.EXE
HOSTBIN := $(BUILD)/host/test_backend
HOSTFORMATBIN := $(BUILD)/host/test_volume_format
HOSTPROBEBIN := $(BUILD)/host/test_tape_probe
HOSTREADERBIN := $(BUILD)/host/test_tape_reader
HOSTNATIVEBIN := $(BUILD)/host/test_native_scan
DOSNATIVEBIN := $(BUILD)/dos/NATTEST.EXE
DISTDIR := $(BUILD)/dist
INSTALL_CFG := installer/INSTALL.$(MAXLANG)
INSTALL_README := installer/README.$(MAXLANG)

WXBIN   := $(WATCOM)/binl64
WXROOT  := $(abspath $(WXWINDOWS))
WXCXX   := $(WXBIN)/wpp
WXCC    := $(WXBIN)/wcc
WXRC    := $(WXBIN)/wrc
WXLINK  := $(WXBIN)/wlink
WXLIB   := $(WXWINDOWS)/build/lib/wx.lib
WXFLAGS := -zq -fr -zw -ml -zt256 -bt=windows -dwx_msw -w1 -d1 \
	-dLANG_$(MAXLANG) \
	-i=$(WXROOT)/include/msw -i=$(WXROOT)/include/base \
	-i=$(WXROOT)/contrib/fafa -i=$(WATCOM)/h -i=$(WATCOM)/h/win

CFLAGS  ?= -std=c90 -Wall -Wextra -Werror -pedantic -O2
CPPFLAGS += -Iinclude
CPPFLAGS += -I$(OPENASPI)/include
CPPFLAGS += -DLANG_$(MAXLANG)

.PHONY: all win16 test check dos-status-smoke dos-read-smoke dos-native-smoke languages dist clean

all: $(DOSBIN) $(DOSNATIVEBIN) $(HOSTBIN) $(HOSTFORMATBIN) $(HOSTPROBEBIN) $(HOSTREADERBIN) $(HOSTNATIVEBIN)

test: $(HOSTBIN) $(HOSTFORMATBIN) $(HOSTPROBEBIN) $(HOSTREADERBIN) $(HOSTNATIVEBIN)
	$(HOSTBIN)
	$(HOSTFORMATBIN)
	$(HOSTPROBEBIN)
	$(HOSTREADERBIN)
	$(HOSTNATIVEBIN)

check: all test win16

dos-status-smoke: all
	MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_status.sh

dos-read-smoke: all
	OPENTAPE_READ_TEST=1 MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_status.sh

dos-native-smoke: all
	OPENTAPE_NATIVE_TEST=1 MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_status.sh

win16: $(WINBIN)

languages:
	$(MAKE) MAXLANG=US BUILD=build/US dist
	$(MAKE) MAXLANG=GR BUILD=build/GR dist

$(DOSBIN): src/main.c core/backend.c include/opentape/backend.h \
		include/opentape/messages.h include/opentape/tape_probe.h \
		core/tape_probe.c backends/legacy_tape/legacy_dos.c \
		core/volume_format.c include/opentape/volume_format.h \
		core/tape_reader.c include/opentape/tape_reader.h \
		core/native_scan.c include/opentape/native_scan.h \
		$(OPENASPI)/include/openaspi/aspitape.h dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) \
		MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) \
		$(abspath $@)

$(DOSNATIVEBIN): tests/test_native_scan.c core/native_scan.c \
		core/volume_format.c include/opentape/native_scan.h \
		include/opentape/volume_format.h dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

$(HOSTBIN): tests/test_backend.c core/backend.c include/opentape/backend.h \
		include/opentape/messages.h $(OPENASPI)/include/openaspi/aspitape.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_backend.c core/backend.c

$(HOSTFORMATBIN): tests/test_volume_format.c core/volume_format.c \
		include/opentape/volume_format.h $(OPENASPI)/include/openaspi/types.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_volume_format.c \
		core/volume_format.c

$(HOSTPROBEBIN): tests/test_tape_probe.c core/tape_probe.c \
		include/opentape/tape_probe.h $(OPENASPI)/include/openaspi/aspitape.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_tape_probe.c core/tape_probe.c

$(HOSTREADERBIN): tests/test_tape_reader.c core/tape_reader.c core/tape_probe.c \
		include/opentape/tape_reader.h include/opentape/tape_probe.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_tape_reader.c \
		core/tape_reader.c core/tape_probe.c

$(HOSTNATIVEBIN): tests/test_native_scan.c core/native_scan.c core/volume_format.c \
		include/opentape/native_scan.h include/opentape/volume_format.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_native_scan.c \
		core/native_scan.c core/volume_format.c

$(BUILD)/win16/opentape.obj: frontends/win16/opentape.cpp \
		include/opentape/backend.h include/opentape/messages.h $(WXLIB)
	mkdir -p $(@D)
	cd frontends/win16 && $(abspath $(WXCXX)) $(WXFLAGS) \
		-i=$(abspath include) -fo=$(abspath $@) opentape.cpp

$(BUILD)/win16/backend.obj: core/backend.c include/opentape/backend.h \
		include/opentape/messages.h
	mkdir -p $(@D)
	$(WXCC) $(WXFLAGS) -i=$(abspath include) -fo=$@ $<

$(BUILD)/win16/opentape.res: frontends/win16/opentape.rc $(WXLIB)
	mkdir -p $(@D)
	cd frontends/win16 && $(abspath $(WXRC)) -q -r -bt=windows \
		-i=$(abspath $(WXWINDOWS)/include/msw) \
		-i=$(abspath $(WXWINDOWS)/contrib/fafa) -i=$(WATCOM)/h/win \
		-fo=$(abspath $@) opentape.rc

$(WINBIN): $(BUILD)/win16/opentape.obj $(BUILD)/win16/backend.obj \
		$(BUILD)/win16/opentape.res $(WXLIB)
	cd $(@D) && $(abspath $(WXLINK)) option quiet format windows \
		name $(notdir $@) libpath $(WATCOM)/lib286/win \
		libpath $(WATCOM)/lib286 library windows \
		file opentape.obj file backend.obj library $(abspath $(WXLIB)) \
		library commdlg library mmsystem library shell library ddeml \
		library ctl3dv2 library $(WATCOM)/lib286/win/plibl.lib \
		option stack=16k option heapsize=16k option version=3.10 \
		option resource=opentape.res option map=OTAPEWIN.map

$(WXLIB):
	$(MAKE) -C $(WXWINDOWS) lib

dist: all win16 $(GDI)/build/INSTALL.EXE $(INSTALL_CFG) \
		$(INSTALL_README) LICENSE
	rm -rf $(DISTDIR)
	mkdir -p $(DISTDIR)
	cp $(DOSBIN) $(WINBIN) $(GDI)/build/INSTALL.EXE LICENSE $(DISTDIR)/
	cp $(INSTALL_CFG) $(DISTDIR)/INSTALL.INI
	cp $(INSTALL_README) $(DISTDIR)/README.TXT
	mv $(DISTDIR)/LICENSE $(DISTDIR)/LICENSE.TXT

$(GDI)/build/INSTALL.EXE:
	$(MAKE) -C $(GDI)

clean:
	rm -rf $(BUILD)
