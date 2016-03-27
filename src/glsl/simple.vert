#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// in variables and uniforms
layout (location = 0) in ivec2 aPos;
layout (location = 1) in vec4 aRenderData;

layout (std140, binding = 1) uniform bufferVals {
  mat4 mvp;
  vec3 cameraPos;
  float terrainSmallestGeometryLodDistance;
  int terrainMaxLoadLevel;
} uniforms;

// out variables
layout (location = 0) out vec2 vTexCoord;
out gl_PerVertex {
  vec4 gl_Position;
};

// constants and aliases
const float kMorphEnd = 0.95, kMorphStart = 0.65;
vec2 terrainOffset = aRenderData.xy;
float terrainLevel = aRenderData.z;
float terrainScale = pow(2, terrainLevel);
int terrainFace = int(aRenderData.w);


vec2 MorphVertex(vec2 vertex, float morph) {
  return vertex - fract(vertex * 0.5) * 2.0 * morph;
}

float GetHeight(vec2 pos) {
  return 0.0;
}

vec2 NodeLocal2Global(vec2 nodeCoord) {
  return terrainOffset + terrainScale * nodeCoord;
}

float EstimateDistance(vec2 geomPos) {
  float estHeight = GetHeight(geomPos);
  vec3 estPos = vec3(geomPos.x, estHeight, geomPos.y);
  vec3 estDiff = uniforms.cameraPos - estPos;
  return length(estDiff);
}

vec4 ModelPos(vec2 m_pos) {
  vec2 pos = NodeLocal2Global(m_pos);
  float dist = EstimateDistance(pos);
  float morph = 0;

  if (terrainLevel < uniforms.terrainMaxLoadLevel) {
    float nextLevelSize = 2 * terrainScale * uniforms.terrainSmallestGeometryLodDistance;
    float maxDist = kMorphEnd * nextLevelSize;
    float startDist = kMorphStart * nextLevelSize;
    morph = smoothstep(startDist, maxDist, dist);

    vec2 morphed_pos = MorphVertex(m_pos, morph);
    pos = NodeLocal2Global(morphed_pos);
  }

  float height = GetHeight(pos);
  return vec4(pos.x, height, pos.y, morph);
}

void main() {
  vTexCoord = vec2(0);
  vec4 modelPos = ModelPos(aPos);
  gl_Position = uniforms.mvp * vec4(modelPos.xyz, 1);

  // GL->VK conventions
  gl_Position.y = -gl_Position.y;
}
