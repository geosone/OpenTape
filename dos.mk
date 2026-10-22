# DOS build through Open386MAX's shared OpenWatcom/JWasm build system.
OPEN386MAX ?= deps/Open386MAX
OPENASPI   ?= deps/OpenASPI
PROJ       := OpenTape
ROOT       := $(abspath $(OPEN386MAX))
SRCDIRS    := src core backends/legacy_tape
ASMINC     := include $(abspath $(OPENASPI))/include $(ROOT)/INCLUDE $(ROOT)/INC \
	$(ROOT)/QUILIB $(ROOT)/386MAX
CASEMAP    := mx
CMODEL     := l
CDEFS      := DOS_VERSION
PROGS      := OPENTAPE.EXE

OPENTAPE.EXE_OBJS  := MAIN.OBJ BACKEND.OBJ VOLUME.OBJ PROBE.OBJ LEGACY.OBJ READER.OBJ
OPENTAPE.EXE_LIBS  := $(ROOT)/build/QUILIB/LQUILIBR.LIB
OPENTAPE.EXE_STACK := 9000
MAIN.OBJ_SRC       := main.c
BACKEND.OBJ_SRC    := backend.c
VOLUME.OBJ_SRC     := volume_format.c
PROBE.OBJ_SRC      := tape_probe.c
LEGACY.OBJ_SRC     := legacy_dos.c
READER.OBJ_SRC     := tape_reader.c

$(ROOT)/linux/bin/jwasm $(ROOT)/linux/bin/jwlink:
	$(MAKE) -C $(ROOT) -f GNUmakefile tools

$(ROOT)/build/QUILIB/LQUILIBR.LIB:
	$(MAKE) -C $(ROOT) -f GNUmakefile \
		OUT=$(ROOT)/build/QUILIB BINDIR=$(ROOT)/build/bin QUILIB

include $(ROOT)/linux/mk/dos.mk
