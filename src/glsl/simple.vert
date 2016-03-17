#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (std140, binding = 1) uniform bufferVals {
    mat4 mvp;
} myBufferVals;

layout (location = 0) in ivec2 pos;
// layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec2 texCoord;
out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
  texCoord = vec2(0);
  gl_Position = myBufferVals.mvp * vec4(pos.x, 0, pos.y, 1);

  // GL->VK conventions
  gl_Position.y = -gl_Position.y;
  // gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
}
