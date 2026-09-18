/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */
#ifndef MIDLESS_ATTACHMENT_H
#define MIDLESS_ATTACHMENT_H
#include "raylib.h"
#include <stdint.h>

// Zero means detached. Other values are an entity index plus one.
typedef struct Attachment {
    int parent;
    Vector3 offset, rotation;
    bool inheritRotation;
} Attachment;

Vector3 Attachment_Position(Vector3 position, Vector3 rotation, Vector3 offset);
Vector3 Attachment_Rotation(Vector3 parent, Vector3 local);
#define ATTACHMENT_NONE 65534
#define ATTACHMENT_LOCAL 65535
#define ATTACHMENT_PACKET_SIZE 34
#define CONTROL_STATE_PACKET_SIZE 7
#define CONTROL_INPUT_PACKET_SIZE 10
#endif
