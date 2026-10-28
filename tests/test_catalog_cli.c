/* Fault-inject transaction boundaries, not the wire codec. A verified tape
 * close and all listing output must precede publication; input never opens tape.
 * Include the production runner to intercept stdio without production hooks. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "opentape/catalog_cli.h"
#include "opentape/catalog_file.h"
#include "opentape/archive_dos.h"
static unsigned loads, saves, prepares, reads, text_calls, fail_text, flushes, fail_flush;
static int prepare_error, load_error, read_error, save_error;
static char transcript[4096];
static int test_text(const char *format, va_list args)
{
    char line[512];
    int size;
    ++text_calls;
    if (text_calls == fail_text) return -1;
    size = vsprintf(line, format, args);
    assert(size >= 0 && strlen(transcript) + (unsigned)size < sizeof(transcript));
    strcat(transcript, line); return size;
}
static int test_printf(const char *format, ...)
{
    va_list args;
    int rc;
    va_start(args, format); rc = test_text(format, args); va_end(args); return rc;
}
static int test_puts(const char *text) { return test_printf("%s\n", text); }
static int test_fprintf(FILE *file, const char *format, ...)
{
    (void)file; (void)format; return 0;
}
static int test_fflush(FILE *file) { (void)file; return ++flushes == fail_flush ? EOF : 0; }
static int test_ferror(FILE *file) { (void)file; return 0; }
#define printf test_printf
#define puts test_puts
#define fprintf test_fprintf
#define fflush test_fflush
#define ferror test_ferror
#include "../src/catalog_cli.c"
#undef printf
#undef puts
#undef fprintf
#undef fflush
#undef ferror
static void capture(struct ot_catalog *c, struct ot_volume_label *label)
{
    struct ot_file_metadata m;
    memset(&m, 0, sizeof(m)); m.kind = OT_FILE_REGULAR; m.file_size = 35000ul;
    strcpy(m.path, "DATA/INPUT.BIN");
    assert(ot_catalog_capture(c, 1ul, 1ul, &m) == 0);
    memset(label, 0, sizeof(*label)); label->version = 1u; label->volume_sequence = 1ul;
}
int ot_dos_catalog_file_load(struct ot_catalog *c, struct ot_volume_label *label,
                             const char *name, unsigned *error)
{
    assert(!strcmp(name, "INPUT.CAT")); ++loads; *error = 0u;
    if (!load_error) capture(c, label);
    return load_error;
}
int ot_dos_selection_prepare(struct ot_dos_selection_export *out, const char *name)
{
    assert(!strcmp(name, "OUTPUT.CAT")); ++prepares; memset(out, 0, sizeof(*out));
    out->dos_error = prepare_error ? 80u : 0u; return prepare_error;
}
int ot_dos_catalog_file_save(const struct ot_catalog *c, const struct ot_volume_label *label,
                             const char *name, unsigned *error)
{
    assert(!strcmp(name, "OUTPUT.CAT") && c->count == 1u && label->version == 1u);
    assert(strstr(transcript, "DATA/INPUT.BIN") != 0 && flushes == 2u);
    ++saves; *error = 0u; return save_error;
}
int ot_dos_archive_verify(const char *device, oa_u8 *buffer, unsigned capacity,
                           ot_metadata_visitor visitor, void *context, struct ot_verify_session_result *result)
{
    assert(!strcmp(device, "NRMT0") && buffer && capacity == OT_BLOCK_RECOMMENDED_SIZE);
    assert(prepares == 1u && !loads && !saves && flushes == 1u);
    assert(visitor == ot_catalog_capture); ++reads;
    /* Simulate metadata received before a late verifier/filemark/close failure. */
    capture((struct ot_catalog *)context, &result->verify.framing.label);
    return read_error;
}
static void reset(void)
{
    loads = saves = prepares = reads = text_calls = fail_text = flushes = fail_flush = 0u;
    prepare_error = load_error = read_error = save_error = 0; *transcript = 0;
}
int main(void)
{
    unsigned i;
    reset(); assert(ot_dos_catalog_list("INPUT.CAT", 0, "NRMT0") == 0);
    assert(loads == 1u && !prepares && !reads && !saves);
    assert(strstr(transcript, "OFFLINE") && strstr(transcript, "DATA/INPUT.BIN"));
    reset(); assert(ot_dos_catalog_list(0, "OUTPUT.CAT", "NRMT0") == 0);
    assert(!loads && prepares == 1u && reads == 1u && saves == 1u);
    reset(); prepare_error = OT_RESTORE_IO;
    assert(ot_dos_catalog_list(0, "OUTPUT.CAT", "NRMT0") == 2 && !reads && !saves);
    reset(); load_error = OT_CATALOG_FILE_FORMAT;
    assert(ot_dos_catalog_list("INPUT.CAT", 0, "NRMT0") == 2 && !reads && !saves && !*transcript);
    reset(); read_error = OT_VERIFY_INCOMPLETE;
    assert(ot_dos_catalog_list(0, "OUTPUT.CAT", "NRMT0") == 2 && !saves);
    assert(strstr(transcript, "DATA/INPUT.BIN") == 0);
    for (i = 1u; i <= 6u; ++i) {
        reset(); fail_text = i;
        assert(ot_dos_catalog_list(0, "OUTPUT.CAT", "NRMT0") == 2);
        /* Last message follows publication, earlier output failures block it. */
        assert(saves == (i == 6u ? 1u : 0u));
        if (i == 1u) assert(!reads);
    }
    reset(); fail_flush = 1u;
    assert(ot_dos_catalog_list(0, "OUTPUT.CAT", "NRMT0") == 2 && !reads && !saves);
    reset(); fail_flush = 2u;
    assert(ot_dos_catalog_list(0, "OUTPUT.CAT", "NRMT0") == 2 && reads == 1u && !saves);
    reset(); save_error = OT_CATALOG_FILE_IO;
    assert(ot_dos_catalog_list(0, "OUTPUT.CAT", "NRMT0") == 2 && saves == 1u);
    reset();
    assert(ot_dos_catalog_list(0, 0, "NRMT0") == 1);
    assert(ot_dos_catalog_list("INPUT.CAT", "OUTPUT.CAT", "NRMT0") == 1);
    assert(ot_dos_catalog_list("RMT0.CAT", 0, "NRMT0") == 1);
    assert(ot_dos_catalog_list(0, "OUTPUT.CAT", "RMT0") == 1);
    assert(!loads && !prepares && !reads && !saves);
    puts("catalogue CLI: offline isolation, late read failure and stdout/publication boundaries OK");
    return 0;
}
