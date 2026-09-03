#include <limits.h>
#include <stdlib.h>
#include "chunkdata.h"

bool ChunkData_IsValidPosition(int x, int y, int z) {
    return x >= 0 && x < CHUNK_SIZE_X &&
           y >= 0 && y < CHUNK_SIZE_Y &&
           z >= 0 && z < CHUNK_SIZE_Z;
}

int ChunkData_PositionToIndex(int x, int y, int z) {
    return (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X + x;
}

void ChunkData_IndexToPosition(int index, int *x, int *y, int *z) {
    *x = index % CHUNK_SIZE_X;
    *y = index / CHUNK_SIZE_XZ;
    *z = (index / CHUNK_SIZE_X) % CHUNK_SIZE_Z;
}

long int ChunkData_PackPosition(int x, int y, int z) {
    return (long)(x & 4095) << 20 | (long)(z & 4095) << 8 | (long)(y & 255);
}

unsigned short *ChunkData_CreateCompressed(
    const unsigned short data[CHUNK_SIZE], int *compressedLength) {
    unsigned short *compressed = malloc(CHUNK_SIZE * 2 * sizeof(*compressed));
    if (compressed == NULL) return NULL;

    unsigned short previous = data[0];
    int count = 1;
    int length = 0;
    for (int i = 1; i <= CHUNK_SIZE; i++) {
        unsigned short current = i == CHUNK_SIZE ? 0 : data[i];
        if (previous != current || count >= USHRT_MAX || i == CHUNK_SIZE) {
            compressed[length++] = previous;
            compressed[length++] = (unsigned short)count;
            previous = current;
            count = 0;
        }
        count++;
    }

    *compressedLength = length;
    unsigned short *resized = realloc(compressed, length * sizeof(*compressed));
    return resized == NULL ? compressed : resized;
}

bool ChunkData_Decompress(
    unsigned short data[CHUNK_SIZE], const unsigned short *compressed,
    int compressedLength) {
    int output = 0;
    if (compressedLength < 0 || compressedLength % 2 != 0) return false;

    for (int i = 0; i < compressedLength; i += 2) {
        int count = compressed[i + 1];
        if (output + count > CHUNK_SIZE) return false;
        for (int j = 0; j < count; j++) data[output++] = compressed[i];
    }
    return output == CHUNK_SIZE;
}
