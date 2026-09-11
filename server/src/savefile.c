#include "savefile.h"
#include <stdio.h>
#include <string.h>

static bool FileMatches(const char *path, const void *data, size_t size) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    const unsigned char *bytes = data;
    unsigned char buffer[4096];
    size_t offset = 0;
    bool matches = true;
    while (offset < size) {
        size_t count = size - offset;
        if (count > sizeof(buffer)) count = sizeof(buffer);
        if (fread(buffer, 1, count, file) != count || memcmp(buffer, bytes + offset, count)) {
            matches = false;
            break;
        }
        offset += count;
    }
    if (matches) matches = fgetc(file) == EOF && !ferror(file);
    fclose(file);
    return matches;
}
#if defined(OS_WINDOWS)
#include <io.h>
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#else
#include <unistd.h>
#endif

bool SaveFile_WriteAtomic(const char *path, const void *data, size_t size) {
    // Existing identical bytes are already saved. Avoid another durable flush
    // and rename for terrain, metadata, or inventories which did not change.
    if (FileMatches(path, data, size)) return true;
    char temporary[256];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= (int)sizeof(temporary)) return false;
    FILE *file = fopen(temporary, "wb");
    if (!file) return false;
    bool ok = fwrite(data, 1, size, file) == size;
    if (fflush(file)) ok = false;
#if defined(OS_WINDOWS)
    if (_commit(_fileno(file))) ok = false;
#else
    if (fsync(fileno(file))) ok = false;
#endif
    if (fclose(file)) ok = false;
    if (!ok) return false;
#if defined(OS_WINDOWS)
    return MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, path) == 0;
#endif
}
