#include <assert.h>
#include <dos.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include "opentape/restore.h"
#include "opentape/source_file.h"

/* Only disposable RBC00..RBC07 directories on the cloned DOS test floppy. */
static void check_file(const char *path, const char *text, unsigned size)
{
    char data[8];
    int handle;
    unsigned got;
    assert(_dos_open(path, O_RDONLY, &handle) == 0u);
    assert(_dos_read(handle, data, sizeof(data), &got) == 0u && got == size);
    assert(memcmp(data, text, size) == 0 && _dos_close(handle) == 0u);
}
int main(void)
{
    struct ot_dos_restore disk;
    struct ot_file_metadata m;
    const char *selection[1];
    char root[16], destination[32], previous[32];
    unsigned test, accepted, flags, error, replacing, failing;
    int handle;
    selection[0] = "FILE.BIN";
    for (test = 0u; test < 8u; ++test) {
        sprintf(root, "RBC%02u", test);
        sprintf(destination, "%s\\FILE.BIN", root);
        sprintf(previous, "%s\\" OT_RESTORE_OLD, root);
        assert(mkdir(root) == 0);
        replacing = test == 1u || test == 3u || test == 4u || test == 5u;
        failing = test == 2u || test == 3u || test == 4u || test == 5u || test == 7u;
        if (replacing) {
            assert(_dos_creatnew(destination, 0u, &handle) == 0u);
            assert(_dos_write(handle, "old", 3u, &accepted) == 0u && accepted == 3u);
            assert(_dos_close(handle) == 0u);
            assert(_dos_setfileattr(destination, 0x27u) == 0u);
        }
        memset(&disk, 0, sizeof(disk)); memset(&m, 0, sizeof(m));
        m.kind = OT_FILE_REGULAR; m.file_size = test >= 6u ? 0ul : 5ul;
        m.dos_attributes = 0x20u; strcpy(m.path, "FILE.BIN");
        assert(ot_dos_restore_open(&disk, root, selection, 1u) == 0);
        assert(disk.ops.begin(&disk, &m) == 0u);
        if (m.file_size != 0ul)
            assert(disk.ops.write(&disk, (const oa_u8 *)"hello", 5u, &accepted) == 0u && accepted == 5u);
        /* Corrupt accepted staging bytes behind the backend's CRC counter. */
        if (test == 2u || test == 3u) {
            assert(lseek(disk.handle, 0L, SEEK_SET) == 0L);
            assert(_dos_write(disk.handle, "X", 1u, &accepted) == 0u && accepted == 1u);
        }
        if (test == 4u) assert(chsize(disk.handle, 4L) == 0);
        if (test == 5u || test == 7u)
            assert(_dos_write(disk.handle, "X", 1u, &accepted) == 0u && accepted == 1u);
        error = disk.ops.commit(&disk, &m);
        assert(disk.opened == 0u);
        assert(_dos_getfileattr(previous, &flags) == 2u);
        if (failing) {
            assert(error != 0u && disk.readback_error == OT_SOURCE_CONTENT);
            assert(disk.readback_files == 0u && disk.replaced == 0u);
            assert(_dos_getfileattr(disk.temporary, &flags) == 0u);
            if (replacing) {
                check_file(destination, "old", 3u);
                assert(_dos_getfileattr(destination, &flags) == 0u && flags == 0x27u);
            } else assert(_dos_getfileattr(destination, &flags) == 2u);
        } else {
            assert(error == 0u && disk.readback_files == 1u && disk.readback_error == 0);
            assert(disk.replaced == replacing);
            assert(_dos_getfileattr(disk.temporary, &flags) == 2u);
            check_file(destination, "hello", (unsigned)m.file_size);
        }
        assert(disk.ops.abort(&disk) == 0u); /* Closed once, no recovery cleanup. */
    }
    puts("OpenTape staged restore readback tests passed");
    return 0;
}
