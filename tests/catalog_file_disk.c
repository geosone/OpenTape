/* Exercise the production DOS streaming backend beyond a 64 KiB file, including
 * independent staging readback/publication. No tape session is opened. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/catalog_file.h"
int main(void)
{
    struct ot_catalog catalog, loaded;
    struct ot_volume_label label, destination;
    struct ot_file_metadata metadata;
    struct ot_catalog_file_info info;
    unsigned i, j, error;
    memset(&catalog, 0, sizeof(catalog)); memset(&loaded, 0, sizeof(loaded));
    memset(&label, 0, sizeof(label)); label.version = 1u; label.volume_sequence = 1ul;
    strcpy(label.volume_name, "LARGE");
    for (i = 0u; i < 256u; ++i) {
        memset(&metadata, 0, sizeof(metadata)); metadata.kind = OT_FILE_REGULAR;
        metadata.file_size = 35000ul + i;
        for (j = 0u; j < 26u; ++j) strcat(metadata.path, "ABCDEFGH/");
        sprintf(metadata.path + strlen(metadata.path), "F%03u.BIN", i);
        assert(ot_catalog_capture(&catalog, 1ul, i + 1ul, &metadata) == 0);
    }
    assert(ot_catalog_mark(&catalog, 0u, 1u) == 0 && ot_catalog_replace(&catalog, 0u, 1u) == 0);
    assert(ot_catalog_file_measure(&catalog, &label, &info) == 0 && info.size > 65535ul);
    assert(ot_dos_catalog_file_save(&catalog, &label, "OPENTAPE.CAT", &error) == 0 && !error);
    assert(ot_dos_catalog_file_load(&loaded, &destination, "OPENTAPE.CAT", &error) == 0 && !error);
    assert(loaded.count == 256u && !loaded.marked && !loaded.replacements);
    assert(!strcmp(loaded.last->metadata.path, catalog.last->metadata.path));
    assert(!strcmp(destination.volume_name, "LARGE"));
    assert(ot_dos_catalog_file_save(&catalog, &label, "OPENTAPE.CAT", &error) != 0 && error == 80u);
    assert(ot_dos_catalog_file_load(&loaded, &destination, "RMT0.CAT", &error) == OT_CATALOG_ARGUMENT && !error);
    assert(loaded.count == 256u && catalog.marked == 1u && catalog.replacements == 1u);
    ot_catalog_free(&catalog); ot_catalog_free(&loaded);
    puts("DOS catalogue file >64K streaming readback and atomic load passed");
    return 0;
}
