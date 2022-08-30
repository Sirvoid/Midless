/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

"#version 330\n"
"in vec3 vertexPosition;"
"in vec2 vertexTexCoord;"
"in float vertexColor;"
"uniform mat4 mvp;"
"out vec2 fragTexCoord;"
"out vec4 fragColor;"
"out vec4 vertPos;"
"out vec4 sunFragColor;"
"void main() {"
"    fragTexCoord = vertexTexCoord / 256;"
"    fragColor = vec4((int(vertexColor) >> 4) / 15.0, (int(vertexColor) >> 4) / 15.0, (int(vertexColor) >> 4) / 15.0, 1.0);"
"    sunFragColor = vec4((int(vertexColor) & 15) / 15.0, (int(vertexColor) & 15) / 15.0, (int(vertexColor) & 15) / 15.0, 1.0);" 
"    vec3 pos = vec3(vertexPosition.x / 15, vertexPosition.y / 15, vertexPosition.z / 15);"
"    vertPos = mvp*vec4(pos, 1.0);"
"    gl_Position = vertPos;"
"}"