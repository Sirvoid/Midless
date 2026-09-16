/**
 * Copyright (c) 2026 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_ENTITY_MODEL_SHADER_H
#define MIDLESS_ENTITY_MODEL_SHADER_H

#if defined(PLATFORM_WEB)
#define ENTITY_SHADER_VERSION "#version 100\nprecision highp float;\n"
#define ENTITY_ATTRIBUTE "attribute "
#define ENTITY_OUT "varying "
#define ENTITY_IN "varying "
#define ENTITY_TEXTURE "texture2D"
#define ENTITY_FRAGMENT_OUTPUT ""
#define ENTITY_FRAGMENT_COLOR "gl_FragColor"
#else
#define ENTITY_SHADER_VERSION "#version 330\n"
#define ENTITY_ATTRIBUTE "in "
#define ENTITY_OUT "out "
#define ENTITY_IN "in "
#define ENTITY_TEXTURE "texture"
#define ENTITY_FRAGMENT_OUTPUT "out vec4 finalColor;"
#define ENTITY_FRAGMENT_COLOR "finalColor"
#endif

static const char *entityVertexShader =
    ENTITY_SHADER_VERSION
    ENTITY_ATTRIBUTE "vec3 vertexPosition;"
    ENTITY_ATTRIBUTE "vec2 vertexTexCoord;"
    ENTITY_ATTRIBUTE "vec4 vertexTangent;"
    ENTITY_ATTRIBUTE "vec4 vertexColor;"
    ENTITY_OUT "vec2 faceUV;"
    ENTITY_OUT "vec4 atlasRect;"
    ENTITY_OUT "vec4 fragColor;"
    "uniform mat4 mvp;"
    "void main() {"
    "faceUV = vertexTexCoord; atlasRect = vertexTangent; fragColor = vertexColor;"
    "gl_Position = mvp * vec4(vertexPosition, 1.0);"
    "}";

static const char *entityFragmentShader =
    ENTITY_SHADER_VERSION
    ENTITY_IN "vec2 faceUV;"
    ENTITY_IN "vec4 atlasRect;"
    ENTITY_IN "vec4 fragColor;"
    ENTITY_FRAGMENT_OUTPUT
    "uniform sampler2D texture0;"
    "uniform vec4 colDiffuse;"
    "void main() {"
    "vec2 uv = atlasRect.xy + fract(faceUV) * atlasRect.zw;"
    ENTITY_FRAGMENT_COLOR " = " ENTITY_TEXTURE "(texture0, uv) * colDiffuse * fragColor;"
    "}";

#endif
