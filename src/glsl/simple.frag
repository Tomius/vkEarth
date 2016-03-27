#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 vTexCoord;
uniform sampler2D tex;

layout (location = 0) out vec4 outColor;

void main() {
  outColor = texture(tex, vTexCoord);
}
