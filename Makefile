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
HOSTPAYLOADBIN := $(BUILD)/host/test_record_payload
DOSPAYLOADBIN := $(BUILD)/dos/PAYTEST.EXE
HOSTVERIFYBIN := $(BUILD)/host/test_archive_verify
DOSVERIFYBIN := $(BUILD)/dos/VERTEST.EXE
HOSTWRITERBIN := $(BUILD)/host/test_archive_writer
HOSTOUTPUTBIN := $(BUILD)/host/test_tape_output
DOSWRITERBIN := $(BUILD)/dos/WRITEST.EXE
DOSOUTPUTBIN := $(BUILD)/dos/OUTTEST.EXE
DOSTAPEBIN := $(BUILD)/dos/TAPETEST.EXE
HOSTSOURCEBIN := $(BUILD)/host/test_source_file
DOSSOURCEBIN := $(BUILD)/dos/SRCTEST.EXE
DOSFILEREADBIN := $(BUILD)/dos/FILEREAD.EXE
HOSTPLANBIN := $(BUILD)/host/test_backup_plan
DOSPLANBIN := $(BUILD)/dos/PLANTEST.EXE
DOSTREEREADBIN := $(BUILD)/dos/TREEREAD.EXE
HOSTSTREAMBIN := $(BUILD)/host/test_verify_stream
DOSSTREAMBIN := $(BUILD)/dos/STRMTEST.EXE
HOSTRESTOREBIN := $(BUILD)/host/test_restore
DOSRESTOREBIN := $(BUILD)/dos/RESTEST.EXE
DOSRESTORECMPBIN := $(BUILD)/dos/RSTCMP.EXE
HOSTSELECTIONBIN := $(BUILD)/host/test_restore_selection
DOSSELECTIONBIN := $(BUILD)/dos/SELTEST.EXE
DOSPROBEBIN := $(BUILD)/dos/PRBTEST.EXE
DOSREADBACKBIN := $(BUILD)/dos/RBTEST.EXE
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

# Native API is optional when building against an older pinned OpenASPI.
ifneq ($(wildcard $(OPENASPI)/include/openaspi/tape_api.h),)
CPPFLAGS += -DOT_NATIVE_TAPE_API
HOSTNATIVEAPIBIN := $(BUILD)/host/test_native_api
HOSTNATIVEARCHIVEBIN := $(BUILD)/host/test_native_archive
NATIVEARCHIVESOURCES := tests/test_native_archive.c core/archive_native.c core/tape_native_api.c \
 core/tape_output.c core/tape_reader.c core/tape_probe.c core/archive_writer.c core/archive_verify.c \
 core/verify_stream.c core/record_payload.c core/native_scan.c core/volume_format.c \
 $(OPENASPI)/common/tape/tape_api.c
all: $(HOSTNATIVEARCHIVEBIN)
test: native-archive-test
.PHONY: native-archive-test dos-native-archive-smoke
native-archive-test: $(HOSTNATIVEARCHIVEBIN)
	$(HOSTNATIVEARCHIVEBIN)
$(HOSTNATIVEARCHIVEBIN): $(NATIVEARCHIVESOURCES) $(wildcard include/opentape/*.h) $(OPENASPI)/include/openaspi/tape_api.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(NATIVEARCHIVESOURCES)
dos-native-archive-smoke:
	MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_native_archive.sh
all: $(HOSTNATIVEAPIBIN)
test: native-api-test
.PHONY: native-api-test dos-native-api-smoke
.PHONY: dos-native-space-smoke
dos-native-space-smoke:
	MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_native_space.sh
dos-native-api-smoke:
	MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_native_api.sh
native-api-test: $(HOSTNATIVEAPIBIN)
	$(HOSTNATIVEAPIBIN)
$(HOSTNATIVEAPIBIN): tests/test_native_api.c core/tape_native_api.c $(OPENASPI)/common/tape/tape_api.c \
		include/opentape/native_tape.h $(OPENASPI)/include/openaspi/tape_api.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_native_api.c core/tape_native_api.c $(OPENASPI)/common/tape/tape_api.c
endif

.PHONY: all win16 test check dos-status-smoke dos-read-smoke dos-native-smoke languages dist clean

all: $(HOSTOUTPUTBIN) $(DOSOUTPUTBIN) $(DOSTAPEBIN) $(HOSTSOURCEBIN) $(DOSSOURCEBIN) $(DOSFILEREADBIN) $(DOSTREEREADBIN) $(DOSPLANBIN)
test: $(HOSTOUTPUTBIN) $(HOSTSOURCEBIN) $(HOSTPLANBIN)

all: $(HOSTSTREAMBIN) $(DOSSTREAMBIN)
test: $(HOSTSTREAMBIN)

all: $(HOSTRESTOREBIN) $(DOSRESTOREBIN)
test: $(HOSTRESTOREBIN)

all: $(HOSTSELECTIONBIN) $(DOSSELECTIONBIN)
all: $(DOSPROBEBIN)
all: $(DOSREADBACKBIN)
test: $(HOSTSELECTIONBIN)

$(DOSPROBEBIN): tests/test_tape_probe.c core/tape_probe.c $(wildcard include/opentape/*.h) dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

$(DOSREADBACKBIN): tests/restore_readback.c backends/legacy_tape/legacy_dos.c core/source_file.c \
		$(wildcard include/opentape/*.h) dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

$(HOSTSELECTIONBIN): tests/test_restore_selection.c core/restore_select.c core/record_payload.c \
		core/volume_format.c $(wildcard include/opentape/*.h)
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_restore_selection.c core/restore_select.c core/record_payload.c core/volume_format.c

$(DOSSELECTIONBIN): tests/test_restore_selection.c core/restore_select.c core/record_payload.c \
		core/volume_format.c $(wildcard include/opentape/*.h) dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

$(DOSTAPEBIN) $(DOSFILEREADBIN) $(DOSTREEREADBIN) $(DOSRESTORECMPBIN) $(DOSBIN) $(DOSRESTOREBIN): core/restore_select.c core/restore_exchange.c

all: $(DOSRESTORECMPBIN)

$(DOSRESTORECMPBIN): tests/restore_compare.c backends/legacy_tape/legacy_dos.c \
		core/backup_plan.c core/source_file.c core/tape_probe.c core/archive_writer.c \
		core/archive_verify.c core/native_scan.c core/record_payload.c core/volume_format.c \
		$(wildcard include/opentape/*.h) dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

.PHONY: dos-restore-smoke
dos-restore-smoke: all
	MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_restore.sh

.PHONY: dos-replace-smoke
dos-replace-smoke: all
	MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_replace.sh

.PHONY: dos-list-smoke
dos-list-smoke: all
	MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_list.sh

.PHONY: dos-catalog-smoke
dos-catalog-smoke: all
	OPENTAPE_TAPE_TEST=tree-catalog MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_status.sh

.PHONY: dos-filter-smoke
dos-filter-smoke: all
	OPENTAPE_TAPE_TEST=tree-filter OPENTAPE_TREE_LARGE=1 MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_status.sh

.PHONY: dos-preview-smoke
dos-preview-smoke: all
	OPENTAPE_TAPE_TEST=tree-preview OPENTAPE_TREE_LARGE=1 MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_status.sh

.PHONY: dos-readback-smoke
dos-readback-smoke: all
	OPENTAPE_TAPE_TEST=tree-readback OPENTAPE_TREE_LARGE=1 MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_status.sh

.PHONY: dos-compare-smoke
dos-compare-smoke: all
	$(MAKE) MAXLANG=$(MAXLANG) dos-readback-smoke
	OPENTAPE_TAPE_TEST=tree-compare OPENTAPE_TREE_LARGE=1 MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_status.sh

.PHONY: dos-devices-smoke
dos-devices-smoke: all
	OPENTAPE_TAPE_TEST=tree-devices OPENTAPE_TREE_LARGE=1 MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_status.sh

.PHONY: dos-ui-smoke
dos-ui-smoke: all
	OPENTAPE_TAPE_TEST=tree-ui OPENTAPE_TREE_LARGE=1 MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_status.sh

.PHONY: dos-ui-export-smoke
dos-ui-export-smoke: all
	OPENTAPE_TAPE_TEST=tree-ui OPENTAPE_TREE_LARGE=1 OPENTAPE_UI_EXPORT=1 MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_status.sh

$(HOSTRESTOREBIN): tests/test_restore.c core/restore.c core/restore_select.c core/restore_exchange.c core/archive_writer.c core/archive_verify.c \
		core/native_scan.c core/record_payload.c core/volume_format.c $(wildcard include/opentape/*.h)
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_restore.c core/restore.c core/restore_select.c core/restore_exchange.c core/archive_writer.c \
		core/archive_verify.c core/native_scan.c core/record_payload.c core/volume_format.c

$(DOSRESTOREBIN): tests/test_restore.c core/restore.c core/archive_writer.c core/archive_verify.c \
		core/native_scan.c core/record_payload.c core/volume_format.c $(wildcard include/opentape/*.h) dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

$(HOSTSTREAMBIN): tests/test_verify_stream.c core/archive_catalog.c core/restore_select.c core/verify_stream.c core/tape_reader.c core/tape_probe.c \
		core/archive_writer.c core/archive_verify.c core/native_scan.c core/record_payload.c \
		core/volume_format.c $(wildcard include/opentape/*.h)
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_verify_stream.c core/verify_stream.c \
		core/tape_reader.c core/tape_probe.c core/archive_writer.c core/archive_verify.c \
		core/native_scan.c core/record_payload.c core/volume_format.c core/archive_catalog.c core/restore_select.c

$(DOSSTREAMBIN): tests/test_verify_stream.c core/archive_catalog.c core/restore_select.c core/verify_stream.c core/tape_reader.c core/tape_probe.c \
		core/archive_writer.c core/archive_verify.c core/native_scan.c core/record_payload.c \
		core/volume_format.c $(wildcard include/opentape/*.h) dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

$(DOSPLANBIN): tests/test_backup_plan.c core/backup_plan.c core/source_file.c core/archive_writer.c \
		core/archive_verify.c core/native_scan.c core/record_payload.c core/volume_format.c \
		$(wildcard include/opentape/*.h) dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

$(DOSTREEREADBIN): tests/tree_readback.c backends/legacy_tape/legacy_dos.c core/backup_plan.c \
		core/source_file.c core/archive_writer.c core/tape_reader.c core/tape_probe.c \
		core/archive_verify.c core/native_scan.c core/record_payload.c core/volume_format.c \
		$(wildcard include/opentape/*.h) dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

.PHONY: dos-tree-smoke
dos-tree-smoke: all
	MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_tree.sh

$(HOSTPLANBIN): tests/test_backup_plan.c core/backup_plan.c core/source_file.c \
		core/archive_writer.c core/archive_verify.c core/native_scan.c core/record_payload.c \
		core/volume_format.c $(wildcard include/opentape/*.h)
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_backup_plan.c core/backup_plan.c core/source_file.c \
		core/archive_writer.c core/archive_verify.c core/native_scan.c core/record_payload.c core/volume_format.c

$(HOSTSOURCEBIN): tests/test_source_file.c core/source_file.c core/archive_writer.c \
		core/archive_verify.c core/native_scan.c core/record_payload.c core/volume_format.c \
		$(wildcard include/opentape/*.h)
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_source_file.c core/source_file.c \
		core/archive_writer.c core/archive_verify.c core/native_scan.c core/record_payload.c core/volume_format.c

$(DOSSOURCEBIN): tests/test_source_file.c core/source_file.c core/archive_writer.c \
		core/archive_verify.c core/native_scan.c core/record_payload.c core/volume_format.c \
		$(wildcard include/opentape/*.h) dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

$(DOSFILEREADBIN): tests/file_readback.c backends/legacy_tape/legacy_dos.c \
		core/backup_plan.c core/source_file.c core/archive_writer.c \
		core/tape_reader.c core/tape_probe.c core/archive_verify.c core/native_scan.c \
		core/record_payload.c core/volume_format.c $(wildcard include/opentape/*.h) dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

.PHONY: dos-file-smoke
dos-file-smoke: all
	MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_file.sh

$(HOSTOUTPUTBIN): tests/test_tape_output.c core/tape_output.c core/tape_probe.c \
		include/opentape/tape_output.h include/opentape/tape_probe.h \
		include/opentape/archive_writer.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_tape_output.c core/tape_output.c core/tape_probe.c

$(DOSOUTPUTBIN): tests/test_tape_output.c core/tape_output.c core/tape_probe.c \
		include/opentape/tape_output.h include/opentape/tape_probe.h \
		include/opentape/archive_writer.h dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

$(DOSTAPEBIN): tests/tape_native.c core/tape_output.c core/tape_reader.c \
		core/backup_plan.c core/source_file.c \
		core/tape_probe.c core/archive_writer.c core/archive_verify.c \
		core/native_scan.c core/record_payload.c core/volume_format.c \
		backends/legacy_tape/legacy_dos.c $(wildcard include/opentape/*.h) dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

.PHONY: dos-write-smoke
dos-write-smoke: all $(DOSTAPEBIN)
	MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_write.sh

all: $(DOSBIN) $(DOSNATIVEBIN) $(DOSPAYLOADBIN) $(DOSVERIFYBIN) $(DOSWRITERBIN) $(HOSTBIN) $(HOSTFORMATBIN) $(HOSTPROBEBIN) $(HOSTREADERBIN) $(HOSTNATIVEBIN) $(HOSTPAYLOADBIN) $(HOSTVERIFYBIN) $(HOSTWRITERBIN)

test: $(HOSTBIN) $(HOSTFORMATBIN) $(HOSTPROBEBIN) $(HOSTREADERBIN) $(HOSTNATIVEBIN) $(HOSTPAYLOADBIN) $(HOSTVERIFYBIN) $(HOSTWRITERBIN)
	$(HOSTBIN)
	$(HOSTFORMATBIN)
	$(HOSTPROBEBIN)
	$(HOSTREADERBIN)
	$(HOSTNATIVEBIN)
	$(HOSTPAYLOADBIN)
	$(HOSTVERIFYBIN)
	$(HOSTWRITERBIN)
	$(HOSTOUTPUTBIN)
	$(HOSTSOURCEBIN)
	$(HOSTPLANBIN)
	$(HOSTSTREAMBIN)
	$(HOSTRESTOREBIN)
	$(HOSTSELECTIONBIN)

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
		src/restore_dos.c core/restore.c include/opentape/restore.h \
		core/verify_stream.c core/archive_catalog.c include/opentape/verify_stream.h \
		core/backup_plan.c include/opentape/backup_plan.h \
		src/backup_dos.c core/source_file.c include/opentape/source_file.h \
		include/opentape/messages.h include/opentape/tape_probe.h \
		core/tape_probe.c backends/legacy_tape/legacy_dos.c \
		core/volume_format.c include/opentape/volume_format.h \
		core/tape_reader.c include/opentape/tape_reader.h \
		core/tape_output.c include/opentape/tape_output.h \
		core/native_scan.c include/opentape/native_scan.h \
		core/record_payload.c include/opentape/record_payload.h core/wire.h \
		core/archive_verify.c include/opentape/archive_verify.h \
		core/archive_writer.c include/opentape/archive_writer.h \
		$(OPENASPI)/include/openaspi/aspitape.h dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) \
		MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) \
		$(abspath $@)

$(DOSNATIVEBIN): tests/test_native_scan.c core/native_scan.c \
		core/volume_format.c core/record_payload.c core/wire.h include/opentape/native_scan.h \
		include/opentape/record_payload.h \
		include/opentape/volume_format.h dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

$(DOSPAYLOADBIN): tests/test_record_payload.c core/record_payload.c core/native_scan.c \
		core/volume_format.c core/wire.h include/opentape/record_payload.h \
		include/opentape/native_scan.h include/opentape/volume_format.h dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

$(DOSVERIFYBIN): tests/test_archive_verify.c core/archive_verify.c \
		core/record_payload.c core/native_scan.c core/volume_format.c core/wire.h \
		include/opentape/archive_verify.h include/opentape/record_payload.h \
		include/opentape/native_scan.h include/opentape/volume_format.h dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

$(DOSWRITERBIN): tests/test_archive_writer.c core/archive_writer.c core/archive_verify.c \
		core/record_payload.c core/native_scan.c core/volume_format.c core/wire.h \
		include/opentape/archive_writer.h include/opentape/archive_verify.h \
		include/opentape/record_payload.h include/opentape/native_scan.h \
		include/opentape/volume_format.h dos.mk
	$(MAKE) -f dos.mk OPEN386MAX=$(abspath $(OPEN386MAX)) \
		OPENASPI=$(abspath $(OPENASPI)) MAXLANG=$(MAXLANG) \
		OUT=$(abspath $(BUILD)/dos-obj) BINDIR=$(abspath $(BUILD)/dos) $(abspath $@)

$(HOSTBIN): tests/test_backend.c core/backend.c include/opentape/backend.h \
		include/opentape/messages.h $(OPENASPI)/include/openaspi/aspitape.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_backend.c core/backend.c

$(HOSTFORMATBIN): tests/test_volume_format.c core/volume_format.c core/wire.h \
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
		core/record_payload.c core/wire.h include/opentape/record_payload.h \
		include/opentape/native_scan.h include/opentape/volume_format.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_native_scan.c \
		core/native_scan.c core/volume_format.c core/record_payload.c

$(HOSTPAYLOADBIN): tests/test_record_payload.c core/record_payload.c core/volume_format.c \
		core/native_scan.c core/wire.h include/opentape/record_payload.h \
		include/opentape/native_scan.h include/opentape/volume_format.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_record_payload.c \
		core/record_payload.c core/volume_format.c core/native_scan.c

$(HOSTVERIFYBIN): tests/test_archive_verify.c core/archive_verify.c \
		core/native_scan.c core/volume_format.c core/record_payload.c core/wire.h \
		include/opentape/archive_verify.h include/opentape/native_scan.h \
		include/opentape/volume_format.h include/opentape/record_payload.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_archive_verify.c \
		core/archive_verify.c core/native_scan.c core/volume_format.c core/record_payload.c

$(HOSTWRITERBIN): tests/test_archive_writer.c core/archive_writer.c core/archive_verify.c \
		core/native_scan.c core/volume_format.c core/record_payload.c core/wire.h \
		include/opentape/archive_writer.h include/opentape/archive_verify.h \
		include/opentape/native_scan.h include/opentape/record_payload.h include/opentape/volume_format.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_archive_writer.c core/archive_writer.c \
		core/archive_verify.c core/native_scan.c core/volume_format.c core/record_payload.c

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

.PHONY: dos-native-media-smoke
dos-native-media-smoke:
	MAXLANG=$(MAXLANG) OPENASPI=$(abspath $(OPENASPI)) bash tests/dos_native_media.sh
