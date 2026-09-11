#include "worldgen.h"
#include <math.h>
#include <string.h>

static float EvaluateCoordinate(WGEval *context, int input, float defaultCoordinate) {
    if (input < 0)
        return defaultCoordinate;
    return Worldgen_Eval(context, input);
}

static uint32_t ToSeedComponent(float value) {
    // Clamp before truncating to keep the conversion within the signed 32-bit range.
    double clamped = fmax(-2147483648.0, fmin(2147483647.0, value));
    return (uint32_t)(int64_t)clamped;
}

void Worldgen_EvalInit(WGEval *context, Vector3 position, Vector3 origin) {
    // Values are only read when their cache stamp matches. Clearing them too
    // doubles the memory traffic for every column and feature evaluation.
    memset(context->cachedVersions, 0, sizeof(context->cachedVersions));
    context->step = context->steps = 0;
    context->position = position;
    context->origin = origin;
    context->evaluationVersion = 1;
}

void Worldgen_EvalY(WGEval *context, float y) {
    context->position.y = y;
    context->evaluationVersion++;
}

/* Lazy, memoized graph evaluation. Select only evaluates the chosen branch;
 * column values survive y changes. Geometry and terrain share these primitives. */
float Worldgen_Eval(WGEval *context, int fieldIndex) {
    if (fieldIndex < 0)
        return 0;
    WGField *field = &worldgen.fields[fieldIndex];
    // Column-only fields remain valid when y changes. Other cached values
    // belong to the current evaluation epoch and must be recomputed.
    int cacheStamp = field->isColumnConstant ? -1 : context->evaluationVersion;
    if (context->cachedVersions[fieldIndex] == cacheStamp)
        return context->values[fieldIndex];
    float result = 0;
    switch (field->op) {
        case WG_CONSTANT:
            result = field->value;
            break;
        case WG_X:
            result = context->position.x;
            break;
        case WG_Y:
            result = context->position.y;
            break;
        case WG_Z:
            result = context->position.z;
            break;
        case WG_ORIGIN_X:
            result = context->origin.x;
            break;
        case WG_ORIGIN_Y:
            result = context->origin.y;
            break;
        case WG_ORIGIN_Z:
            result = context->origin.z;
            break;
        case WG_STEP:
            result = context->step;
            break;
        case WG_STEPS:
            result = context->steps;
            break;
        case WG_LOCAL_INDEX: {
            int x = (int)context->position.x % CHUNK_SIZE_X,
                y = (int)context->position.y % CHUNK_SIZE_Y,
                z = (int)context->position.z % CHUNK_SIZE_Z;
            if (x < 0)
                x += CHUNK_SIZE_X;
            if (y < 0)
                y += CHUNK_SIZE_Y;
            if (z < 0)
                z += CHUNK_SIZE_Z;
            result = (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X + x;
            break;
        }
        case WG_ADD:
            result = Worldgen_Eval(context, field->firstInput) +
                     Worldgen_Eval(context, field->secondInput);
            break;
        case WG_SUB:
            result = Worldgen_Eval(context, field->firstInput) -
                     Worldgen_Eval(context, field->secondInput);
            break;
        case WG_MUL:
            result = Worldgen_Eval(context, field->firstInput) *
                     Worldgen_Eval(context, field->secondInput);
            break;
        case WG_DIV: {
            float numerator = Worldgen_Eval(context, field->firstInput);
            float denominator = Worldgen_Eval(context, field->secondInput);
            result = denominator == 0 ? 0 : numerator / denominator;
            break;
        }
        case WG_MOD: {
            float numerator = Worldgen_Eval(context, field->firstInput);
            float denominator = Worldgen_Eval(context, field->secondInput);
            result = denominator == 0 ? 0 : fmodf(numerator, denominator);
            break;
        }
        case WG_MIN:
            result = fminf(Worldgen_Eval(context, field->firstInput),
                           Worldgen_Eval(context, field->secondInput));
            break;
        case WG_MAX:
            result = fmaxf(Worldgen_Eval(context, field->firstInput),
                           Worldgen_Eval(context, field->secondInput));
            break;
        case WG_ABS:
            result = fabsf(Worldgen_Eval(context, field->firstInput));
            break;
        case WG_LT:
            result = Worldgen_Eval(context, field->firstInput) <
                     Worldgen_Eval(context, field->secondInput);
            break;
        case WG_EQ:
            result = Worldgen_Eval(context, field->firstInput) ==
                     Worldgen_Eval(context, field->secondInput);
            break;
        // Keep this conditional lazy: the unused branch may contain expensive noise.
        case WG_SELECT:
            result = Worldgen_Eval(context, field->firstInput) != 0
                         ? Worldgen_Eval(context, field->secondInput)
                         : Worldgen_Eval(context, field->thirdInput);
            break;
        case WG_FLOOR:
            result = floorf(Worldgen_Eval(context, field->firstInput));
            break;
        case WG_CEIL:
            result = ceilf(Worldgen_Eval(context, field->firstInput));
            break;
        case WG_TRUNC:
            result = truncf(Worldgen_Eval(context, field->firstInput));
            break;
        case WG_SIN:
            result = sinf(Worldgen_Eval(context, field->firstInput));
            break;
        case WG_COS:
            result = cosf(Worldgen_Eval(context, field->firstInput));
            break;
        case WG_RANDOM: {
            /* Portable 32-bit LCG. Integer seed mixing avoids float precision
             * loss when a world seed is combined with a coordinate expression. */
            uint32_t seed = ToSeedComponent(Worldgen_Eval(context, field->firstInput));
            uint32_t salt = ToSeedComponent(Worldgen_Eval(context, field->secondInput));
            seed += (uint32_t)worldgen.seed * (uint32_t)field->seedScale +
                    salt * (uint32_t)field->saltScale;
            result = ((seed * 214013u + 2531011u) >> 16) & 32767u;
            break;
        }
        case WG_NOISE2: {
            float x = EvaluateCoordinate(context, field->firstInput, context->position.x);
            float z = EvaluateCoordinate(context, field->secondInput, context->position.z);
            result = fnlGetNoise2D(&field->noise, x, z);
            break;
        }
        case WG_NOISE3: {
            float x = EvaluateCoordinate(context, field->firstInput, context->position.x);
            float y = EvaluateCoordinate(context, field->secondInput, context->position.y);
            float z = EvaluateCoordinate(context, field->thirdInput, context->position.z);
            result = fnlGetNoise3D(&field->noise, x, y, z);
            break;
        }
    }
    if (!isfinite(result))
        result = 0;
    context->cachedVersions[fieldIndex] = cacheStamp;
    context->values[fieldIndex] = result;
    return result;
}

float Worldgen_Field(int field, float x, float y, float z) {
    WGEval context;
    Vector3 position = {x, y, z};
    Worldgen_EvalInit(&context, position, position);
    return Worldgen_Eval(&context, field);
}
