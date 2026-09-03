#ifndef MIDLESS_SHARED_CHUNKDATA_H
#define MIDLESS_SHARED_CHUNKDATA_H

#include <stdbool.h>

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 16
#define CHUNK_SIZE_Z 16
#define CHUNK_SIZE_XZ (CHUNK_SIZE_X * CHUNK_SIZE_Z)
#define CHUNK_SIZE (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z)

bool ChunkData_IsValidPosition(int x, int y, int z);
int ChunkData_PositionToIndex(int x, int y, int z);
void ChunkData_IndexToPosition(int index, int *x, int *y, int *z);
long int ChunkData_PackPosition(int x, int y, int z);

unsigned short *ChunkData_CreateCompressed(
    const unsigned short data[CHUNK_SIZE], int *compressedLength);
bool ChunkData_Decompress(
    unsigned short data[CHUNK_SIZE], const unsigned short *compressed,
    int compressedLength);

#endif
