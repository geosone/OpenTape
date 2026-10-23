/* Destructive diagnostic, not distributed. The host harness owns identity and
 * positioning checks. No implicit rewind or automatic write retry. */
#include <stdio.h>
#include <string.h>
#include "opentape/tape_output.h"
#include "opentape/tape_reader.h"

static struct ot_tape_output output;
static struct ot_tape_reader reader;
static struct ot_archive_verify actual;
static unsigned blocks;
static oa_u8 expected_buffer[1200], read_buffer[1200], data[1024];

static int compare_block(void *p, const oa_u8 *b, unsigned n, unsigned *accepted)
{
    unsigned got;
    int rc;
    (void)p;
    *accepted = 0u;
    rc = ot_reader_read(&reader, read_buffer, sizeof(read_buffer), &got);
    if (rc != OT_READ_BLOCK || got != n || memcmp(b, read_buffer, n) != 0)
        return -1;
    rc = ot_archive_verify_block(&actual, read_buffer, (oa_u16)got);
    if (rc != 0) return rc;
    ++blocks;
    *accepted = got;
    return 0;
}

static int compare_finish(void *p)
{
    unsigned got;
    (void)p;
    if (ot_reader_read(&reader, read_buffer, sizeof(read_buffer), &got) != OT_READ_FILEMARK)
        return -1;
    if (ot_archive_verify_finish(&actual) != 0 || blocks != 11u || actual.sessions != 1ul)
        return -1;
    return ot_reader_close(&reader);
}

static int fixture(const struct ot_writer_sink *sink)
{
    struct ot_archive_writer w;
    struct ot_volume_label label;
    struct ot_session_start start;
    struct ot_file_metadata m;
    unsigned i, chunk, offset;
    int rc;
    memset(&w, 0, sizeof(w));
    memset(&label, 0, sizeof(label));
    memset(&start, 0, sizeof(start));
    memset(&m, 0, sizeof(m));
    label.version = OT_VOLUME_FORMAT_VERSION;
    label.volume_sequence = 1ul;
    label.created_utc = 123456789ul;
    strcpy(label.volume_name, "OA2101L8-NATIVE-TEST");
    start.created_utc = label.created_utc;
    strcpy(start.name, "OpenTape DOS roundtrip");
#define STEP(call) do { rc = (call); if (rc != 0) return rc; } while (0)
    STEP(ot_archive_writer_init(&w, sink, expected_buffer, sizeof(expected_buffer)));
    STEP(ot_archive_writer_volume(&w, &label));
    STEP(ot_archive_writer_session(&w, 1ul, &start));
    m.kind = OT_FILE_DIRECTORY;
    m.dos_attributes = 0x10u;
    strcpy(m.path, "TEST");
    STEP(ot_archive_writer_metadata(&w, 1ul, &m));
    m.kind = OT_FILE_REGULAR;
    m.dos_attributes = 0x20u;
    m.file_size = 2500ul;
    strcpy(m.path, "TEST/DATA.BIN");
    STEP(ot_archive_writer_metadata(&w, 2ul, &m));
    for (offset = 0u; offset < 2500u; offset += chunk) {
        chunk = 2500u - offset;
        if (chunk > sizeof(data)) chunk = sizeof(data);
        for (i = 0u; i < chunk; ++i) data[i] = (oa_u8)((offset + i) * 37u + 11u);
        STEP(ot_archive_writer_data(&w, data, chunk));
    }
    STEP(ot_archive_writer_file_end(&w));
    m.file_size = 0ul;
    strcpy(m.path, "TEST/EMPTY.TXT");
    STEP(ot_archive_writer_metadata(&w, 3ul, &m));
    STEP(ot_archive_writer_file_end(&w));
    STEP(ot_archive_writer_session_end(&w));
    return ot_archive_writer_complete(&w);
#undef STEP
}

int main(int argc, char **argv)
{
    struct ot_writer_sink compare;
    int rc, writing;
    /* No defaults for destructive mode, no access before complete validation. */
    writing = argc == 5 && strcmp(argv[1], "--write") == 0 &&
        strcmp(argv[2], "--overwrite") == 0 && strcmp(argv[3], "--device") == 0 &&
        strcmp(argv[4], "NRMT0") == 0;
    if (!writing && !(argc == 4 && strcmp(argv[1], "--verify") == 0 &&
        strcmp(argv[2], "--device") == 0 && strcmp(argv[3], "NRMT0") == 0)) {
        puts("TAPETEST: explicit --write --overwrite --device NRMT0 or --verify --device NRMT0 required");
        return 1;
    }
    if (writing) {
        puts("TAPETEST: OVERWRITING selected tape at current position");
        rc = ot_output_open(&output, ot_dos_output_ops(), "NRMT0", 1u);
        if (rc == 0) rc = fixture(&output.sink);
        if (output.opened) ot_output_close(&output);
        printf("TAPETEST write rc=%d DOS=%u state=%04X driver=%u SCSI=%02X sense=%02X opcode=%04X\n",
               rc, output.dos_error, output.status.tape.current_state,
               output.status.tape.driver_error, output.status.tape.last_scsi_status,
               output.status.tape.last_sense_key, output.status.tape.last_scsi_opcode);
    } else {
        compare.context = 0;
        compare.write_block = compare_block;
        compare.finish_session = compare_finish;
        rc = ot_reader_open(&reader, ot_dos_reader_ops(), "NRMT0");
        if (rc == 0) rc = fixture(&compare);
        if (reader.opened) ot_reader_close(&reader);
        printf("TAPETEST readback rc=%d DOS=%u blocks=%u\n", rc, reader.dos_error, blocks);
    }
    if (rc != 0) return 2;
    puts(writing ? "OpenTape native tape write passed" : "OpenTape native tape readback passed");
    return 0;
}
