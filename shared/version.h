#ifndef MIDLESS_VERSION_H
#define MIDLESS_VERSION_H

#define GAME_VERSION "Pre-Alpha 1.4 dev"
#define GAME_VERSION_TEXT "Midless " GAME_VERSION
#define GAME_PROTOCOL_VERSION 18

// On-disk formats are still in development.
#define CHUNK_FILE_VERSION 1
#define CHUNK_SECTION_VERSION 1
#define PLAYER_INVENTORY_VERSION 1

// Defaults for versions that scripts may override for their own content.
#define WORLDGEN_DEFAULT_VERSION 1
#define METADATA_DEFAULT_VERSION 1

#endif
