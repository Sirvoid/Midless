/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

"#version 330\n"
"layout(location = 0) in vec3 vertexPosition;"
"layout(location = 1) in vec2 vertexTexCoord;"
"layout(location = 3) in float vertexColor;"
"uniform mat4 mvp;"
"uniform mat4 matModelView;"
"out vec2 fragTexCoord;"
"out vec4 fragColor;"
"out float fogDistance;"
"out vec4 sunFragColor;"
"void main() {"
"    fragTexCoord = vertexTexCoord / 256.0;"
"    fragColor = vec4((int(vertexColor) >> 4) / 15.0, (int(vertexColor) >> 4) / 15.0, (int(vertexColor) >> 4) / 15.0, 1.0);"
"    sunFragColor = vec4((int(vertexColor) & 15) / 15.0, (int(vertexColor) & 15) / 15.0, (int(vertexColor) & 15) / 15.0, 1.0);" 
"    vec3 pos = vec3(vertexPosition.x / 15.0, vertexPosition.y / 15.0, vertexPosition.z / 15.0);"
"    fogDistance = length((matModelView * vec4(pos, 1.0)).xyz);"
"    gl_Position = mvp * vec4(pos, 1.0);"
"}"
