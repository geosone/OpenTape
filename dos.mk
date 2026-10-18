# DOS build through Open386MAX's shared OpenWatcom/JWasm build system.
OPEN386MAX ?= ../386MAX
OPENASPI   ?= ../OpenAspi
PROJ       := OpenTape
ROOT       := $(abspath $(OPEN386MAX))
SRCDIRS    := src core
ASMINC     := include $(abspath $(OPENASPI))/include $(ROOT)/INCLUDE $(ROOT)/INC \
	$(ROOT)/QUILIB $(ROOT)/386MAX
CASEMAP    := mx
CMODEL     := l
CDEFS      := DOS_VERSION
PROGS      := OPENTAPE.EXE

OPENTAPE.EXE_OBJS  := MAIN.OBJ BACKEND.OBJ
OPENTAPE.EXE_LIBS  := $(ROOT)/build/QUILIB/LQUILIBR.LIB
OPENTAPE.EXE_STACK := 9000
MAIN.OBJ_SRC       := main.c
BACKEND.OBJ_SRC    := backend.c

$(ROOT)/build/QUILIB/LQUILIBR.LIB:
	$(MAKE) -C $(ROOT)/QUILIB -f GNUmakefile $@

include $(ROOT)/linux/mk/dos.mk
