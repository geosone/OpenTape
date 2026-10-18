# Tape formats

OpenTape has two standard on-tape formats. Device capability, not cartridge
length, decides whether LTFS is possible.

## LTFS

LTFS is offered only when the drive and inserted medium provide the partition
operations required by the LTFS specification. The upstream reference
implementation documents LTO-5 through LTO-9 and IBM enterprise generations
beginning with TS1140; it also states that LTFS uses the tape drive's partition
feature.

OpenTape must write the published LTFS structures and XML index without private
incompatible changes. Reading an existing volume is non-destructive. Creating
or reformatting an LTFS volume requires an explicit warning and confirmation.

LTFS operations use detected OpenASPI extension IOCTLs or direct ASPI because
the historical ASPITAPE interface does not expose partition and logical-block
positioning primitives.

References:

- [LTFS reference implementation](https://github.com/LinearTapeFileSystem/ltfs)
- [SNIA LTFS format specification](https://www.snia.org/tech_activities/standards/curr_standards/ltfs)

## Streaming TAR

DDS/DAT and other drives without LTFS-compatible partitioning use a standard
streaming TAR layout. This preserves interchange with existing tape tools and
works through the historical `RMTx`/`NRMTx` character-device interface.

OpenTape-specific catalogue data stays off tape where possible. Any DOS
metadata that cannot be represented portably must use documented TAR extension
records that older readers can skip. A restore must never depend on the local
catalogue database.

## Automatic selection

`auto` follows these rules:

1. Inspect the existing medium without writing.
2. If a valid LTFS label is present and the drive supports its operations, use
   LTFS.
3. If a TAR stream is present, use TAR.
4. Treat blank or unknown media as unformatted and ask the user to select a
   supported format.
5. Never silently format, partition or overwrite a tape.
