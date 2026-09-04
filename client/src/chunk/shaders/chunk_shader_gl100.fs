/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

"#version 100\n"
"precision mediump float;"
"varying vec2 fragTexCoord;"
"varying vec4 fragColor;"
"varying mediump float fogDistance;"
"varying vec4 sunFragColor;"
"uniform sampler2D texture0;"
"uniform float sunlightStrength;"
"uniform vec3 fogColor;"
"uniform float fogStart;"
"uniform float fogEnd;"
"void main() {"
"   vec4 texelColor = texture2D(texture0, fragTexCoord);"
"   if(texelColor.a == 0.0) discard;"
"   vec4 litColor = texelColor * clamp(sunFragColor * sunlightStrength + fragColor, vec4(0.1, 0.1, 0.1, 1), vec4(1,1,1,1));"
"   float fogAmount = smoothstep(fogStart, fogEnd, fogDistance);"
"   gl_FragColor = vec4(mix(litColor.rgb, fogColor, fogAmount), litColor.a);"
"}"
