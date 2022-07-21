/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_VECTORMATH_H
#define G_VECTORMATH_H

#include "raylib.h"
#include "raymath.h"

/**
 * WARNING!! 
 * This code is a copy of the code from the Raylib codebase,
 * which has not yet been included in the official release.
 * It may be unstable.
 * Each function has a link to the original code located on Github.
 * The code will be deleted after the original code gets into the stable release.
 */

// https://github.com/raysan5/raylib/blob/4a9391ae83757afd86b6f1cccae4335c611b5b41/src/raymath.h#L952
Vector3 Vector3ClampValue(Vector3 v, float min, float max)
{
    Vector3 result = { 0 };

    float length = (v.x*v.x) + (v.y*v.y) + (v.z*v.z);
    if (length > 0.0f)
    {
        length = sqrtf(length);

        if (length < min)
        {
            float scale = min/length;
            result.x = v.x*scale;
            result.y = v.y*scale;
            result.z = v.z*scale;
        }
        else if (length > max)
        {
            float scale = max/length;
            result.x = v.x*scale;
            result.y = v.y*scale;
            result.z = v.z*scale;
        }
        else 
        {
            return v;
        }
    }

    return result;
}

#endif