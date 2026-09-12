/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "binarydata.h"
#include <stdlib.h>
#include <string.h>

#define BINARY_LIMIT (16u * 1024u * 1024u)

void Binary_Write(BinaryWriter *out, const void *data, size_t size) {
    if (out->failed) return;
    if (size > BINARY_LIMIT - out->size || (size && !data)) { out->failed = true; return; }
    size_t needed = out->size + size;
    if (needed > out->capacity) {
        size_t capacity = out->capacity ? out->capacity : 64;
        while (capacity < needed) capacity *= 2;
        uint8_t *grown = realloc(out->data, capacity);
        if (!grown) { out->failed = true; return; }
        out->data = grown;
        out->capacity = capacity;
    }
    if (size) memcpy(out->data + out->size, data, size);
    out->size = needed;
}
void Binary_U8(BinaryWriter *out, uint8_t value) { Binary_Write(out, &value, 1); }
void Binary_U16(BinaryWriter *out, uint16_t value) {
    Binary_U8(out, value); Binary_U8(out, value >> 8);
}
void Binary_U32(BinaryWriter *out, uint32_t value) {
    Binary_U16(out, value); Binary_U16(out, value >> 16);
}
void Binary_VarUInt(BinaryWriter *out, uint32_t value) {
    do {
        uint8_t byte = value & 127;
        value >>= 7;
        Binary_U8(out, byte | (value ? 128 : 0));
    } while (value);
}
void Binary_Float(BinaryWriter *out, float value) {
    uint32_t bits; memcpy(&bits, &value, 4); Binary_U32(out, bits);
}
const uint8_t *Binary_Read(BinaryReader *in, size_t size) {
    if (in->failed || size > in->size - in->offset) { in->failed = true; return NULL; }
    const uint8_t *data = in->data ? in->data + in->offset : NULL;
    in->offset += size;
    return data;
}
uint8_t Binary_ReadU8(BinaryReader *in) { const uint8_t *p = Binary_Read(in, 1); return p ? *p : 0; }
uint16_t Binary_ReadU16(BinaryReader *in) {
    uint16_t low = Binary_ReadU8(in); return low | (uint16_t)Binary_ReadU8(in) << 8;
}
uint32_t Binary_ReadU32(BinaryReader *in) {
    uint32_t low = Binary_ReadU16(in); return low | (uint32_t)Binary_ReadU16(in) << 16;
}
uint32_t Binary_ReadVarUInt(BinaryReader *in) {
    uint32_t value = 0;
    for (int shift = 0; shift <= 28; shift += 7) {
        uint8_t byte = Binary_ReadU8(in);
        if (shift == 28 && (byte & 240)) { in->failed = true; return 0; }
        value |= (uint32_t)(byte & 127) << shift;
        if (!(byte & 128)) return value;
    }
    in->failed = true;
    return 0;
}
float Binary_ReadFloat(BinaryReader *in) {
    uint32_t bits = Binary_ReadU32(in); float value; memcpy(&value, &bits, 4); return value;
}
bool Binary_End(const BinaryReader *in) { return !in->failed && in->offset == in->size; }
