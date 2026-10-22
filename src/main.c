#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <commfunc.h>
#include <input.h>
#include <libcolor.h>
#include <screen.h>
#include <ui.h>
#include <video.h>
#include <windowfn.h>

#include "opentape/backend.h"
#include "opentape/messages.h"
#include "opentape/tape_probe.h"
#include "opentape/tape_reader.h"
#include "opentape/volume_format.h"
#include "opentape/native_scan.h"

static void usage(void)
{
    puts(OT_MSG_VERSION);
    puts(OT_MSG_USAGE);
    puts(OT_MSG_READ_USAGE);
    puts(OT_MSG_SCAN_USAGE);
    puts(OT_MSG_NOT_IMPLEMENTED);
}

static void put_line(WINDESC *panel, int row, char *text, char color)
{
    WINDESC line;

    line = *panel;
    line.row += row;
    line.col += 2;
    line.nrows = 1;
    line.ncols -= 4;
    wput_csa(&line, text, color);
}

static void clear_ui(void)
{
    WINDESC screen;

    SETWINDOW(&screen, 0, 0, ScreenSize.row, ScreenSize.col);
    wput_sca(&screen, VIDWORD(0x07, ' '));
    move_cursor(ScreenSize.row - 1, 0);
    show_cursor();
}

/* Required cleanup callback of QUILIB's shared INT 23h/24h support. */
void abortprep(int flag)
{
    (void)flag;
    end_input();
    clear_ui();
}

static void show_ui(enum ot_backend_kind selected)
{
    char backend[48];

    initializeScreenInfo(FALSE);
    init_input(NULL);
    hide_cursor();
    paintBackgroundScreen();

    put_line(&DataWin, 1, OT_MSG_APP_TITLE, DataYellowColor);
    put_line(&DataWin, 3, OT_MSG_TASK_SUMMARY, DataColor);
    put_line(&DataWin, 5, OT_MSG_NOT_IMPLEMENTED, DataColor);
    sprintf(backend, OT_MSG_BACKEND_FMT, ot_backend_name(selected));
    put_line(&DataWin, 7, backend, DataColor);

    put_line(&QuickWin, 1, OT_MSG_HELP_TITLE, QuickBackColor);
    put_line(&QuickWin, 3, OT_MSG_ESCAPE_DOS, QuickBackColor);

    pressAnyKey(INPUT_WAIT);
    end_input();
    clear_ui();
}

static int print_tape_status(const char *name)
{
    struct ot_tape_status status;
    unsigned dos_error;
    int rc;

    rc = ot_dos_tape_probe(name, &status, &dos_error);
    if (rc != OT_PROBE_OK) {
        fprintf(stderr, OT_MSG_PROBE_ERROR, name, rc, dos_error);
        return 2;
    }
    printf(OT_MSG_PROBE_STATUS, name, (unsigned)status.tape.host_id,
           (unsigned)status.tape.target_id, (unsigned)status.tape.lun,
           (unsigned)status.tape.current_state,
           (unsigned)status.tape.fixed_block_size);
    printf(OT_MSG_PROBE_SCSI, (unsigned)status.tape.last_scsi_status,
           (unsigned)status.tape.last_sense_key,
           (unsigned)status.tape.last_scsi_opcode,
           (unsigned)status.tape.residual_bytes);
    if (status.extended) {
        printf(OT_MSG_PROBE_ABI, 16u, (unsigned)status.tape.driver_error);
    } else {
        puts(OT_MSG_PROBE_LEGACY);
    }
    puts(OT_MSG_PROBE_READONLY);
    return 0;
}

static const char *scan_error_reason(int rc)
{
    switch (rc) {
    case OT_FORMAT_BAD_MAGIC: return OT_MSG_SCAN_MAGIC;
    case OT_FORMAT_BAD_VERSION: return OT_MSG_SCAN_VERSION;
    case OT_FORMAT_BAD_HEADER_CRC:
    case OT_FORMAT_BAD_PAYLOAD_CRC:
    case OT_SCAN_RECORD_CRC: return OT_MSG_SCAN_CRC;
    case OT_SCAN_BAD_SEQUENCE: return OT_MSG_SCAN_SEQUENCE;
    case OT_SCAN_MISSING_LABEL: return OT_MSG_SCAN_NO_LABEL;
    case OT_SCAN_BAD_LABEL: return OT_MSG_SCAN_BAD_LABEL;
    default: return OT_MSG_SCAN_SIZE;
    }
}

static int read_tape_test(const char *name, int native)
{
    struct ot_tape_reader reader;
    struct ot_native_scan scan;
    char label_name[33];
    unsigned char *buffer;
    unsigned count, bytes, capacity, block_size, i;
    unsigned long total;
    int rc, exit_code;

    buffer = malloc(OT_BLOCK_RECOMMENDED_SIZE);
    if (buffer == 0) {
        fputs(OT_MSG_READ_MEMORY, stderr);
        return 2;
    }
    memset(&reader, 0, sizeof(reader));
    rc = ot_reader_open(&reader, ot_dos_reader_ops(), name);
    if (rc != OT_PROBE_OK) {
        fprintf(stderr, OT_MSG_PROBE_ERROR, name, rc, reader.dos_error);
        free(buffer);
        return 2;
    }
    capacity = OT_BLOCK_RECOMMENDED_SIZE;
    block_size = reader.status.tape.fixed_block_size;
    memset(&scan, 0, sizeof(scan));
    if (native && (reader.status.tape.device_parameters & OA_MT_PARAM_FIXED_BLOCK) != 0u) {
        fprintf(stderr, OT_MSG_PROBE_ERROR, name, OT_READ_BAD_MODE, 0u);
        ot_reader_close(&reader);
        free(buffer);
        return 2;                       /* fixed transport reassembly is pending */
    }
    if ((reader.status.tape.device_parameters & OA_MT_PARAM_FIXED_BLOCK) != 0u)
        capacity = (capacity / block_size) * block_size;
    puts(OT_MSG_READ_WARNING);
    total = 0ul;
    exit_code = 3;
    for (count = 0u; count < 16u; ++count) {
        rc = ot_reader_read(&reader, buffer, capacity, &bytes);
        if (rc == OT_READ_FILEMARK) {
            if (native) {
                if (!scan.have_label) {
                    printf(OT_MSG_SCAN_ERROR, OT_SCAN_MISSING_LABEL,
                           scan_error_reason(OT_SCAN_MISSING_LABEL), 0ul);
                    exit_code = 2;
                } else {
                    printf(OT_MSG_SCAN_EOF, (unsigned long)scan.blocks,
                           (unsigned long)scan.records);
                    puts(OT_MSG_SCAN_PARTIAL);
                    exit_code = 0;
                }
                break;
            }
            printf(OT_MSG_READ_EOF, count, total);
            exit_code = 0;
            break;
        }
        if (rc != OT_READ_BLOCK) {
            fprintf(stderr, OT_MSG_PROBE_ERROR, name, rc, reader.dos_error);
            printf(OT_MSG_PROBE_SCSI, (unsigned)reader.status.tape.last_scsi_status,
                   (unsigned)reader.status.tape.last_sense_key,
                   (unsigned)reader.status.tape.last_scsi_opcode,
                   (unsigned)reader.status.tape.residual_bytes);
            exit_code = 2;
            break;
        }
        if (native) {
            rc = ot_native_scan_block(&scan, buffer, (oa_u16)bytes);
            if (rc != OT_FORMAT_OK) {
                printf(OT_MSG_SCAN_ERROR, rc, scan_error_reason(rc),
                       (unsigned long)scan.blocks);
                exit_code = 2;
                break;
            }
            if (scan.blocks == 1ul) {
                memset(label_name, 0, sizeof(label_name));
                for (i = 0u; i < 32u && scan.label.volume_name[i] != 0; ++i) {
                    unsigned char c = (unsigned char)scan.label.volume_name[i];
                    label_name[i] = c >= 32u && c <= 126u ? (char)c : '.';
                }
                printf(OT_MSG_SCAN_LABEL, label_name,
                       (unsigned long)scan.label.volume_sequence);
            }
            printf(OT_MSG_SCAN_BLOCK, (unsigned long)scan.last_block.block_number,
                   (unsigned)scan.last_block.record_count);
        } else {
            printf(OT_MSG_READ_BLOCK, count, bytes,
                   (unsigned long)ot_crc32(buffer, (oa_u16)bytes));
        }
        total += bytes;
    }
    if (exit_code == 3) puts(OT_MSG_READ_LIMIT);
    if (ot_reader_close(&reader) != OT_PROBE_OK) {
        fprintf(stderr, OT_MSG_PROBE_ERROR, name, OT_PROBE_CLOSE_FAILED,
                reader.dos_error);
        exit_code = 2;
    }
    free(buffer);
    return exit_code;
}

int main(int argc, char **argv)
{
    enum ot_backend_kind selected;

    if (argc > 1 && (strcmp(argv[1], "--read-test") == 0 ||
                     strcmp(argv[1], "--scan") == 0)) {
        int native = strcmp(argv[1], "--scan") == 0;
        if (argc == 2) return read_tape_test("NRMT0", native);
        if (argc == 4 && strcmp(argv[2], "--device") == 0 &&
            ot_probe_name_valid(argv[3]) &&
            (argv[3][1] == 'R' || argv[3][1] == 'r'))
            return read_tape_test(argv[3], native);
        usage();
        return 1;
    }
    if (argc > 1 && (strcmp(argv[1], "--status") == 0 ||
                     strcmp(argv[1], "/STATUS") == 0)) {
        if (argc == 2) return print_tape_status("NRMT0");
        if (argc == 4 && strcmp(argv[2], "--device") == 0 &&
            ot_probe_name_valid(argv[3])) return print_tape_status(argv[3]);
        usage();
        return 1;
    }
    if (argc > 2 || (argc > 1 && strcmp(argv[1], "--help") != 0 &&
        strcmp(argv[1], "/?") != 0)) {
        fputs(OT_MSG_UNKNOWN_OPTION, stderr);
        fputc('\n', stderr);
        usage();
        return 1;
    }

    selected = ot_backend_select(0u, OT_BACKEND_AUTO);
    if (argc > 1) {
        usage();
        printf(OT_MSG_BACKEND_FMT "\n", ot_backend_name(selected));
    } else {
        show_ui(selected);
    }
    return 0;
}
