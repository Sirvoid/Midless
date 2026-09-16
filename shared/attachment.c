/* Copyright (c) 2026 Sirvoid. Released under the MIT License. */
#include "attachment.h"
#include "raymath.h"

Vector3 Attachment_Position(Vector3 position, Vector3 rotation, Vector3 offset) {
    return Vector3Add(position, Vector3Transform(offset, MatrixRotateXYZ(rotation)));
}
Vector3 Attachment_Rotation(Vector3 parent, Vector3 local) {
    Matrix m = QuaternionToMatrix(QuaternionMultiply(QuaternionFromMatrix(MatrixRotateXYZ(parent)),
                                                     QuaternionFromMatrix(MatrixRotateXYZ(local))));
    float y = asinf(Clamp(m.m8, -1, 1));
    if (fabsf(cosf(y)) > 0.00001f) return (Vector3){atan2f(-m.m9, m.m10), y, atan2f(-m.m4, m.m0)};
    return (Vector3){atan2f(m.m6, m.m5), y, 0};
}
