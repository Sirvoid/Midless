/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

"#version 330\n"
"in vec2 fragTexCoord;"
"in vec4 fragColor;"
"in float fogDistance;"
"in vec4 sunFragColor;"
"uniform sampler2D texture0;"
"uniform float drawDistance;"
"uniform float sunlightStrength;"
"out vec4 finalColor;"
"float getFog(float d)"
"{"
"   float FogMax = drawDistance - 16.0;"
"   float FogMin = drawDistance - 24.0;"
"   if (d >= FogMax) return 1.0;"
"   if (d <= FogMin) return 0.0;"
"   return 1.0 - (FogMax - d) / (FogMax - FogMin);"
"}"
"void main() {"
"   vec4 texelColor = texture(texture0, fragTexCoord);"
"   if(texelColor.a == 0.0) discard;"
"   float alpha = getFog(fogDistance);"
"   finalColor = (texelColor*clamp(sunFragColor * sunlightStrength + fragColor, vec4(0.1, 0.1, 0.1, 1), vec4(1,1,1,1))) * (1.0 - alpha);"
"}"
