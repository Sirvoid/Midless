#ifndef MIDLESS_ROTATION_H
#define MIDLESS_ROTATION_H
#include <math.h>
#include <stdint.h>
// Internal rotations are XYZ Euler angles in radians. Only packets use bytes.
static inline uint8_t Rotation_Encode(float radians) {
    float turns = fmodf(radians, 6.2831853071795864769f);
    int value = (int)roundf(turns * (256.0f / 6.2831853071795864769f));
    return (uint8_t)(value & 255);
}
static inline float Rotation_Decode(uint8_t angle) {
    int signedAngle = angle < 128 ? angle : (int)angle - 256;
    return signedAngle * (6.2831853071795864769f / 256.0f);
}
static inline float Rotation_Interpolate(float current, float target, float amount) {
    float delta = atan2f(sinf(target-current), cosf(target-current));
    return current + delta * amount;
}
#endif
