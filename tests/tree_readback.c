/* Read-only comparison with the staged DOS tree; does not extract files. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/backup_plan.h"
#include "opentape/tape_reader.h"
int main(int argc, char **argv)
{
    struct ot_backup_plan plan;
    struct ot_plan_entry *e;
    struct ot_dos_source source;
    struct ot_tape_reader reader;
    struct ot_archive_verify verify;
    struct ot_record_header r;
    struct ot_file_metadata m;
    oa_u8 *block, *data;
    unsigned error, n, got, count = 0u, entries = 0u;
    unsigned extra = argc == 2 && strcmp(argv[1], "--large") == 0 ? 40u : 0u;
    int rc = -1, boundary = 0;
    memset(&plan, 0, sizeof(plan)); memset(&source, 0, sizeof(source));
    memset(&reader, 0, sizeof(reader)); memset(&verify, 0, sizeof(verify));
    block = malloc(OT_BLOCK_RECOMMENDED_SIZE); data = malloc(OT_BLOCK_RECOMMENDED_SIZE);
    if (block == 0 || data == 0) goto done;
    if (ot_dos_plan_tree(&plan, "SRC", "ROOT", &error) != 0) goto done;
    e = plan.first;
    if (ot_reader_open(&reader, ot_dos_reader_ops(), "NRMT0") != 0) goto done;
    for (count = 0u; count < 128u; ++count) {
        rc = ot_reader_read(&reader, block, OT_BLOCK_RECOMMENDED_SIZE, &n);
        if (rc == OT_READ_FILEMARK) { boundary = 1; break; }
        if (rc != OT_READ_BLOCK || ot_archive_verify_block(&verify, block, (oa_u16)n) != 0 ||
            ot_record_header_decode(&r, block + OT_BLOCK_HEADER_SIZE) != 0) goto fail;
        if (r.type == OT_RECORD_FILE_METADATA) {
            if (source.opened || e == 0 ||
                ot_file_metadata_decode(&m, block + OT_WRITER_RECORD_OVERHEAD, (oa_u16)r.data_size) != 0 ||
                strcmp(m.path, e->metadata.path) != 0 || m.kind != e->metadata.kind ||
                m.file_size != e->metadata.file_size || m.dos_attributes != e->metadata.dos_attributes) goto fail;
            if (m.kind == OT_FILE_REGULAR) {
                if (m.dos_date != e->metadata.dos_date || m.dos_time != e->metadata.dos_time ||
                    ot_dos_source_open(&source, e->source, e->metadata.path) != 0) goto fail;
            }
            e = e->next; ++entries;
        } else if (r.type == OT_RECORD_FILE_DATA) {
            if (!source.opened || source.ops.read(&source, data, (unsigned)r.data_size, &got) != 0u ||
                got != r.data_size || memcmp(data, block + OT_WRITER_RECORD_OVERHEAD, got) != 0) goto fail;
        } else if (r.type == OT_RECORD_FILE_END) {
            if (!source.opened || source.ops.read(&source, data, 1u, &got) != 0u || got != 0u ||
                source.ops.recheck(&source) != 0u || source.ops.close(&source) != 0u) goto fail;
        }
    }
    if (!boundary || e != 0 || source.opened || entries != 6u + extra ||
        ot_archive_verify_finish(&verify) != 0 || verify.sessions != 1ul ||
        verify.files != 3ul + extra || verify.directories != 3ul) goto fail;
    rc = 0; goto done;
fail:
    rc = -1;
done:
    if (source.opened && source.ops.close(&source) != 0u) rc = -1;
    if (reader.opened && ot_reader_close(&reader) != 0) rc = -1;
    ot_plan_free(&plan); free(block); free(data);
    if (rc != 0) { printf("OpenTape tree readback FAILED at block %u\n", count); return 2; }
    printf("OpenTape tree readback passed: %u entries, %u blocks\n", entries, count);
    return 0;
}
