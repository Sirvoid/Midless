/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

"#version 100\n"
"attribute vec3 vertexPosition;"
"attribute vec2 vertexTexCoord;"
"attribute float vertexColor;"
"uniform mat4 mvp;"
"varying vec2 fragTexCoord;"
"varying vec4 fragColor;"
"varying vec4 vertPos;"
"varying vec4 sunFragColor;"
"void main() {"
"    fragTexCoord = vertexTexCoord / 256.0;"
"    float fC = (float(int(vertexColor) / 16)) / 15.0;"
"    float sC = (vertexColor - float(int(vertexColor / 16.0) * 16)) / 15.0;"
"    fragColor = vec4(fC, fC, fC, 1.0);"
"    sunFragColor = vec4(sC, sC, sC, 1.0);" 
"    vec3 pos = vec3(vertexPosition.x / 15.0, vertexPosition.y / 15.0, vertexPosition.z / 15.0);"
"    vertPos = mvp*vec4(pos, 1.0);"
"    gl_Position = vertPos;"
"}"