#ifndef MIDLESS_SAVE_FILE_H
#define MIDLESS_SAVE_FILE_H
#include <stdbool.h>
#include <stddef.h>
bool SaveFile_WriteAtomic(const char *path, const void *data, size_t size);
#endif
