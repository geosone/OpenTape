# DOS build through Open386MAX's shared OpenWatcom/JWasm build system.
OPEN386MAX ?= deps/Open386MAX
OPENASPI   ?= deps/OpenASPI
PROJ       := OpenTape
ROOT       := $(abspath $(OPEN386MAX))
SRCDIRS    := src core backends/legacy_tape tests
ASMINC     := include $(abspath $(OPENASPI))/include $(ROOT)/INCLUDE $(ROOT)/INC \
	$(ROOT)/QUILIB $(ROOT)/386MAX
CASEMAP    := mx
CMODEL     := l
CDEFS      := DOS_VERSION
PROGS      := OPENTAPE.EXE NATTEST.EXE PAYTEST.EXE VERTEST.EXE

OPENTAPE.EXE_OBJS  := MAIN.OBJ BACKEND.OBJ VOLUME.OBJ PROBE.OBJ LEGACY.OBJ READER.OBJ NATIVE.OBJ PAYLOAD.OBJ VERIFY.OBJ
OPENTAPE.EXE_LIBS  := $(ROOT)/build/QUILIB/LQUILIBR.LIB
OPENTAPE.EXE_STACK := 9000
MAIN.OBJ_SRC       := main.c
BACKEND.OBJ_SRC    := backend.c
VOLUME.OBJ_SRC     := volume_format.c
PROBE.OBJ_SRC      := tape_probe.c
LEGACY.OBJ_SRC     := legacy_dos.c
READER.OBJ_SRC     := tape_reader.c
NATIVE.OBJ_SRC     := native_scan.c
NATTEST.OBJ_SRC    := test_native_scan.c
NATTEST.EXE_OBJS  := NATTEST.OBJ NATIVE.OBJ VOLUME.OBJ PAYLOAD.OBJ
NATTEST.EXE_STACK := 9000
PAYLOAD.OBJ_SRC   := record_payload.c
PAYTEST.OBJ_SRC   := test_record_payload.c
PAYTEST.EXE_OBJS  := PAYTEST.OBJ PAYLOAD.OBJ NATIVE.OBJ VOLUME.OBJ
PAYTEST.EXE_STACK := 9000
VERIFY.OBJ_SRC    := archive_verify.c
VERTEST.OBJ_SRC   := test_archive_verify.c
VERTEST.EXE_OBJS  := VERTEST.OBJ VERIFY.OBJ PAYLOAD.OBJ NATIVE.OBJ VOLUME.OBJ
VERTEST.EXE_STACK := 9000

$(ROOT)/linux/bin/jwasm $(ROOT)/linux/bin/jwlink:
	$(MAKE) -C $(ROOT) -f GNUmakefile tools

$(ROOT)/build/QUILIB/LQUILIBR.LIB:
	$(MAKE) -C $(ROOT) -f GNUmakefile \
		OUT=$(ROOT)/build/QUILIB BINDIR=$(ROOT)/build/bin QUILIB

include $(ROOT)/linux/mk/dos.mk
