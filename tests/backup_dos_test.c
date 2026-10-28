/* The shell supplies a resident memory-only tape and compares the whole DOS
 * disk image afterwards. Unauthorized/invalid/preflight calls must issue no
 * tape CDB; one valid quiet call is independently read back by the CLI. */
#include <stdio.h>
#include <string.h>
#include "opentape/backup_dos.h"
#include "opentape/backup_preview.h"
#include "opentape/archive_dos.h"
#include "opentape/tape_output.h"
struct observed { unsigned calls, failed, intermediate, finalizing; oa_u32 accepted; };
static void progress(void *context, const struct ot_backup_progress *p)
{
    struct observed *state = context;
    ++state->calls;
    if (p->total.files != 1ul || p->total.directories || p->total.bytes_low != 35000ul || p->total.bytes_high ||
        p->accepted_high || p->accepted_low < state->accepted || p->accepted_low > 35000ul) state->failed = 1u;
    state->accepted = p->accepted_low;
    if (p->accepted_low > 0ul && p->accepted_low < 35000ul && p->files == 0ul) state->intermediate = 1u;
    if (p->phase == OT_BACKUP_FINALIZING) {
        ++state->finalizing;
        if (p->files != 1ul || p->accepted_low != 35000ul || strcmp(p->path, "DATA/INPUT.BIN") != 0) state->failed = 1u;
    }
}
static int failed(int code) { printf("Backup service check %d failed\n", code); return code; }
int main(void)
{
    struct ot_dos_backup_request request;
    struct ot_dos_backup_result result;
    struct ot_backup_preview_result preview;
    struct ot_dos_source source;
    struct observed observation;
    memset(&request, 0, sizeof(request)); memset(&observation, 0, sizeof(observation));
#ifdef OT_NATIVE_TAPE_API
    ot_dos_archive_select_native();
#endif
    if (ot_dos_backup_request_valid(&request) || ot_dos_backup_request_valid(0)) return failed(1);
    request.source = "INPUT.BIN"; request.path = "DATA/INPUT.BIN"; request.volume = "TEST";
    if (!ot_dos_backup_request_valid(&request)) return failed(2);
    if (ot_dos_backup_preview(&request, &preview) != 0 || preview.result != 0 || preview.source_error ||
        preview.summary.files != 1ul || preview.summary.directories || preview.summary.bytes_low != 35000ul || preview.summary.bytes_high) return failed(20);
    request.source = "NONE.BIN";
    if (ot_dos_backup_preview(&request, &preview) == 0 || preview.summary.files || preview.summary.bytes_low) return failed(21);
    request.source = "RMT0.CFG";
    if (ot_dos_backup_preview(&request, &preview) != OT_SOURCE_ARGUMENT || preview.summary.files) return failed(22);
    memset(&source, 0, sizeof(source));
    if (ot_dos_source_open(&source, "RMT0.CFG", "INPUT.BIN") != OT_SOURCE_ARGUMENT || source.opened) return failed(23);
    memset(&source, 0, sizeof(source));
    if (ot_dos_source_open(&source, "RMT0\\.", "INPUT.BIN") != OT_SOURCE_ARGUMENT || source.opened) return failed(26);
    request.source = "INPUT.BIN"; request.tree = 1u;
    if (ot_dos_backup_preview(&request, &preview) == 0 || preview.summary.files || preview.summary.directories) return failed(24);
    request.tree = 0u;
    if (ot_dos_backup_preview(0, &preview) != OT_SOURCE_ARGUMENT || ot_dos_backup_preview(&request, 0) != OT_SOURCE_ARGUMENT) return failed(25);
    if (ot_dos_backup_execute(&request, "NRMT0", 0u, &result) != OT_OUTPUT_NOT_AUTHORIZED || result.output_opened) return failed(3);
    if (ot_dos_backup_execute(&request, "NRMT0", 2u, &result) != OT_OUTPUT_NOT_AUTHORIZED || result.finished) return failed(4);
    if (ot_dos_backup_execute(&request, "NRMT0", 1u, 0) != OT_SOURCE_ARGUMENT) return failed(5);
    if (ot_dos_backup_execute(&request, "INPUT.BIN", 1u, &result) != OT_SOURCE_ARGUMENT) return failed(6);
    request.volume = "BAD NAME";
    if (ot_dos_backup_request_valid(&request) || ot_dos_backup_execute(&request, "NRMT0", 1u, &result) != OT_SOURCE_ARGUMENT) return failed(7);
    request.volume = "TEST"; request.tree = 3u;
    if (ot_dos_backup_request_valid(&request)) return failed(8);
    request.tree = 0u; request.path = "../INPUT.BIN";
    if (ot_dos_backup_request_valid(&request)) return failed(9);
    request.path = "DATA/INPUT.BIN"; request.source = "NONE.BIN";
    if (ot_dos_backup_execute(&request, "NRMT0", 1u, &result) == 0 || result.output_opened || result.finished) return failed(10);
    request.source = "INPUT.BIN"; request.tree = 1u;
    if (ot_dos_backup_execute(&request, "NRMT0", 1u, &result) == 0 || result.output_opened) return failed(11);
    request.tree = 0u;
    if (ot_dos_backup_execute(&request, "NRMT3", 1u, &result) == 0 || result.output_opened) return failed(12);
    if (ot_dos_backup_execute_observed(&request, "NRMT0", 0u, progress, &observation, &result) != OT_OUTPUT_NOT_AUTHORIZED || observation.calls) return failed(27);
    request.tree = OT_BACKUP_SET; request.source = "NONE.LST";
    if (!ot_dos_backup_request_valid(&request)) return failed(30);
    if (ot_dos_backup_execute(&request, "NRMT0", 0u, &result) != OT_OUTPUT_NOT_AUTHORIZED || result.output_opened) return failed(31);
    if (ot_dos_backup_execute(&request, "NRMT0", 1u, &result) == 0 || result.output_opened || result.finished) return failed(32);
    if (ot_dos_backup_preview(&request, &preview) == 0 || preview.summary.files || preview.summary.directories) return failed(33);
    request.tree = OT_BACKUP_FILE; request.source = "INPUT.BIN";
    puts("Begin quiet backup service");
    if (ot_dos_backup_execute_observed(&request, "NRMT0", 1u, progress, &observation, &result) != 0 || result.result != 0 ||
        !result.finished || !result.output_opened || result.files != 1ul || result.directories != 0ul ||
        result.bytes_low != 35000ul || result.bytes_high != 0ul || result.source_error || result.tape_error) return failed(13);
    if (ot_dos_backup_execute(&request, "NRMT0", 0u, &result) != OT_OUTPUT_NOT_AUTHORIZED || result.output_opened || result.finished) return failed(14);
    if (observation.failed || observation.calls != 8u || !observation.intermediate || observation.finalizing != 1u) return failed(28);
    if (ot_dos_backup_execute_observed(&request, "NRMT0", 0u, progress, &observation, &result) != OT_OUTPUT_NOT_AUTHORIZED || observation.calls != 8u) return failed(29);
    puts("Quiet backup service passed");
    return 0;
}
