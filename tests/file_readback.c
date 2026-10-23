/* Read-only byte comparison against a real DOS source, not a restore tool. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/source_file.h"
#include "opentape/tape_reader.h"

int main(void)
{
    struct ot_dos_source source;
    struct ot_tape_reader reader;
    struct ot_archive_verify verify;
    struct ot_record_header record;
    struct ot_file_metadata metadata;
    oa_u8 *block, *data;
    unsigned n, got, count = 0u, stage = 0u;
    oa_u32 bytes = 0ul;
    int rc = -1, boundary = 0;
    memset(&source, 0, sizeof(source));
    memset(&reader, 0, sizeof(reader));
    memset(&verify, 0, sizeof(verify));
    block = malloc(OT_BLOCK_RECOMMENDED_SIZE);
    data = malloc(OT_BLOCK_RECOMMENDED_SIZE);
    if (block == 0 || data == 0) goto done;
    stage = 1u;
    if (ot_dos_source_open(&source, "SOURCE.BIN", "FILE.BIN") != 0) goto done;
    stage = 2u;
    if (ot_reader_open(&reader, ot_dos_reader_ops(), "NRMT0") != 0) goto done;
    for (count = 0u; count < 16u; ++count) {
        stage = 3u;
        rc = ot_reader_read(&reader, block, OT_BLOCK_RECOMMENDED_SIZE, &n);
        if (rc == OT_READ_FILEMARK) { boundary = 1; break; }
        if (rc != OT_READ_BLOCK || ot_archive_verify_block(&verify, block, (oa_u16)n) != 0) goto fail;
        if (ot_record_header_decode(&record, block + OT_BLOCK_HEADER_SIZE) != 0) goto fail;
        if (record.type == OT_RECORD_FILE_METADATA) {
            stage = 4u;
            if (ot_file_metadata_decode(&metadata, block + OT_WRITER_RECORD_OVERHEAD,
                                         (oa_u16)record.data_size) != 0 ||
                strcmp(metadata.path, "FILE.BIN") != 0 ||
                metadata.file_size != source.metadata.file_size ||
                metadata.dos_attributes != source.metadata.dos_attributes ||
                metadata.dos_date != source.metadata.dos_date ||
                metadata.dos_time != source.metadata.dos_time) goto fail;
        }
        if (record.type == OT_RECORD_FILE_DATA) {
            stage = 5u;
            got = 0u;
            if (source.ops.read(&source, data, (unsigned)record.data_size, &got) != 0u ||
                got != record.data_size ||
                memcmp(data, block + OT_WRITER_RECORD_OVERHEAD, got) != 0) goto fail;
            bytes += got;
        }
    }
    stage = 6u;
    if (!boundary || ot_archive_verify_finish(&verify) != 0 || verify.sessions != 1ul ||
        verify.files != 1ul || verify.directories != 0ul || bytes != source.metadata.file_size) goto fail;
    if (source.ops.read(&source, data, 1u, &got) != 0u || got != 0u ||
        source.ops.recheck(&source) != 0u) goto fail;
    rc = 0;
    goto done;
fail:
    rc = -1;
done:
    if (source.opened && source.ops.close(&source) != 0u) rc = -1;
    if (reader.opened && ot_reader_close(&reader) != 0) rc = -1;
    free(block);
    free(data);
    if (rc != 0) {
        printf("OpenTape real-file readback FAILED: stage=%u block=%u source DOS=%u tape DOS=%u\n",
               stage, count, source.dos_error, reader.dos_error);
        return 2;
    }
    printf("OpenTape real-file readback passed: %lu bytes\n", (unsigned long)bytes);
    return 0;
}
