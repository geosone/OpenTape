#ifndef OPENTAPE_CATALOG_FILE_H
#define OPENTAPE_CATALOG_FILE_H
#include "opentape/archive_catalog.h"
#include "opentape/source_file.h"
#include "opentape/restore.h"
#define OT_CATALOG_FILE_FORMAT (-190)
#define OT_CATALOG_FILE_IO (-191)
#define OT_CATALOG_FILE_HEADER 32u
#define OT_CATALOG_FILE_MAX (32ul + OT_VOLUME_LABEL_SIZE + 256ul * (12ul + OT_FILE_METADATA_PREFIX + OT_FILE_PATH_MAX))

/* Versioned metadata snapshot only: no backup contents, marks, replacement
 * grants, device state or evidence of the currently mounted cartridge.
 * size includes the header; payload_crc covers label plus encoded entries. */
struct ot_catalog_file_info { oa_u32 size, payload_crc; };
/* Validate nodes, codecs, identity order and annotation counters, then measure
 * without I/O/allocation. Run before acquiring any filesystem output handle. */
int ot_catalog_file_measure(const struct ot_catalog *catalog, const struct ot_volume_label *label,
                             struct ot_catalog_file_info *info);
/* Already-open exclusive staging output. Valid ops close once on every outcome;
 * publish only after all exact writes/close succeed. Keep input immutable. */
int ot_catalog_file_save(const struct ot_catalog *catalog, const struct ot_volume_label *label,
                          const struct ot_selection_output *output);
/* Already-open regular source, closes once after valid ops. EOF, snapshot
 * recheck and checked close precede atomic replacement of both destinations.
 * Failure frees provisional nodes and preserves catalogue/label/annotations.
 * Destinations must be initialized; nodes must be owned by ot_catalog_free.
 * Success confers offline metadata only, never permission to operate on tape. */
int ot_catalog_file_load(struct ot_catalog *catalog, struct ot_volume_label *label,
                          const struct ot_source_ops *source);
#ifdef DOS_VERSION
#include "opentape/dos_ui.h"
/* Returns one only after successful offline import. Filename capacity >=41.
 * Editor, separate one-shot confirmation and result dismissal use QUILIB. */
int ot_dos_catalog_file_ui(struct ot_catalog *, struct ot_volume_label *, char *filename,
                            unsigned exporting, ot_dos_ui_line);
/* Explicit regular-file service; bounded 40-character name with 8.3 leaf.
 * Save is new-file-only staged readback; load preserves destination on failure.
 * Both reject tape/device aliases before opening and never issue tape I/O. */
int ot_dos_catalog_file_save(const struct ot_catalog *, const struct ot_volume_label *, const char *, unsigned *dos_error);
int ot_dos_catalog_file_load(struct ot_catalog *, struct ot_volume_label *, const char *, unsigned *dos_error);
#endif
#endif
