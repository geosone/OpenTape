# Tape formats

OpenTape begins with its own documented, self-describing sequential volume
format. This follows the recovery model used by Bacula/Bareos: a database is a
fast catalogue, not the only copy of the tape contents. The format uses one
ordinary tape partition and therefore covers DDS/DAT and other legacy media.

See [native-volume-format.md](native-volume-format.md) for the byte layout.

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

## TAR interoperability

TAR is a future import/export compatibility path, not OpenTape's native backup
format. It can be added without changing the native media version.

## Automatic selection

Future `auto` selection follows these rules:

1. Inspect the existing medium without writing.
2. Use the native codec for an `OPENTAPE` volume label.
3. If a valid LTFS label is present and the drive supports its operations, use
   LTFS.
4. Offer an explicit importer for recognized TAR media.
5. Treat blank or unknown media as unformatted.
6. Never silently format, partition or overwrite a tape.
