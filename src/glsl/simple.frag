// Copyright (c) 2016, Tamas Csala

#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) flat in int vFace;
layout (location = 1) in vec2 vTexCoord;

uniform sampler2D tex[6];

layout (location = 0) out vec4 outColor;

void main() {
  vec3 color = sqrt(texture(tex[vFace], vTexCoord).rgb);
  outColor = vec4(color, 1);
}
