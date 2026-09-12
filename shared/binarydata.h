/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_BINARY_DATA_H
#define MIDLESS_BINARY_DATA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A bounded reader and a growing writer. Errors stick until the buffer is freed.
typedef struct BinaryWriter { uint8_t *data; size_t size, capacity; bool failed; } BinaryWriter;
typedef struct BinaryReader { const uint8_t *data; size_t size, offset; bool failed; } BinaryReader;
void Binary_Write(BinaryWriter *out, const void *data, size_t size);
void Binary_U8(BinaryWriter *out, uint8_t value);
void Binary_U16(BinaryWriter *out, uint16_t value);
void Binary_U32(BinaryWriter *out, uint32_t value);
void Binary_VarUInt(BinaryWriter *out, uint32_t value);
void Binary_Float(BinaryWriter *out, float value);
const uint8_t *Binary_Read(BinaryReader *in, size_t size);
uint8_t Binary_ReadU8(BinaryReader *in);
uint16_t Binary_ReadU16(BinaryReader *in);
uint32_t Binary_ReadU32(BinaryReader *in);
uint32_t Binary_ReadVarUInt(BinaryReader *in);
float Binary_ReadFloat(BinaryReader *in);
bool Binary_End(const BinaryReader *in);
#endif
