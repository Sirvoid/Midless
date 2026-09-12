#ifndef MIDLESS_METADATA_INTERNAL_H
#define MIDLESS_METADATA_INTERNAL_H
#include "metadata.h"
#include "binarydata.h"
#include "itemdefinition.h"

// Shared schema and encoding details used by the C implementation and Lua conversion.
#define METADATA_MAX_SCHEMAS 512
#define METADATA_MAX_FIELDS 64
typedef enum MetadataFieldType {
    FIELD_UINT,
    FIELD_INT,
    FIELD_BOOL,
    FIELD_FLOAT,
    FIELD_STRING,
    FIELD_INVENTORY
} MetadataFieldType;
typedef struct MetadataField {
    char name[65];
    MetadataFieldType type;
    int bits, limit;
    double defaultNumber;
    char defaultString[257];
} MetadataField;
typedef struct MetadataSchema {
    uint16_t version;
    int count;
    MetadataField fields[METADATA_MAX_FIELDS];
    Metadata defaults;
} MetadataSchema;
extern MetadataSchema *serverMetadataSchemas[METADATA_MAX_SCHEMAS];
extern int serverMetadataSchemaCount, serverBlockSchemas[256];
extern int serverItemSchemas[ITEM_LIMIT];

typedef struct MetadataBits {
    uint8_t byte;
    int count;
} MetadataBits;
void MetadataCodec_WriteBits(BinaryWriter *out, MetadataBits *bits, uint32_t value, int count);
void MetadataCodec_FlushBits(BinaryWriter *out, MetadataBits *bits);
uint32_t MetadataCodec_ReadBits(BinaryReader *in, MetadataBits *bits, int count);
void MetadataCodec_WriteDefault(BinaryWriter *out, MetadataBits *bits, const MetadataField *field);
bool MetadataCodec_BuildDefaults(MetadataSchema *schema);
bool MetadataCodec_SkipField(const MetadataField *field, BinaryReader *in, MetadataBits *bits);
bool MetadataCodec_ReadField(const MetadataField *field, BinaryReader *in, MetadataBits *bits);
int ServerMetadata_Register(const MetadataSchema *layout);
void ServerMetadata_Reset(void);

#endif
