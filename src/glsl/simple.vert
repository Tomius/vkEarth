#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// layout (std140, binding = 0) uniform bufferVals {
//     mat4 mvp;
// } myBufferVals;

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 inColor;

layout (location = 0) out vec2 outColor;
out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
  outColor = inColor;
  gl_Position = vec4(pos, 1);

  // // GL->VK conventions
  // gl_Position.y = -gl_Position.y;
  // gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
}
