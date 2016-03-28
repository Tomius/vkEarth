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
  float terrainSphereRadius;
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

/* Cube 2 Sphere */

const int kPosX = 0;
const int kNegX = 1;
const int kPosY = 2;
const int kNegY = 3;
const int kPosZ = 4;
const int kNegZ = 5;

float sqr(float x) {
  return x * x;
}

vec3 Spherify(vec3 p) {
  // return p;
  return vec3(
    p.x * sqrt(1 - sqr(p.y)/2 - sqr(p.z)/2 + sqr(p.y*p.z)/3),
    p.y * sqrt(1 - sqr(p.z)/2 - sqr(p.x)/2 + sqr(p.z*p.x)/3),
    p.z * sqrt(1 - sqr(p.x)/2 - sqr(p.y)/2 + sqr(p.x*p.y)/3)
  );
}

float Radius() {
  return uniforms.terrainSphereRadius;
}

vec3 WorldPos(vec3 pos) {
  float height = pos.y; pos.y = 0;
  pos = (pos - Radius()) / (Radius());
  switch (terrainFace) {
    case kPosX: pos = vec3(-pos.y, -pos.z, -pos.x); break;
    case kNegX: pos = vec3(+pos.y, -pos.z, +pos.x); break;
    case kPosY: pos = vec3(-pos.z, -pos.y, +pos.x); break;
    case kNegY: pos = vec3(+pos.z, +pos.y, +pos.x); break;
    case kPosZ: pos = vec3(-pos.x, -pos.z, +pos.y); break;
    case kNegZ: pos = vec3(+pos.x, -pos.z, -pos.y); break;
  }
  return (Radius() + height) * Spherify(pos);
}

/* Cube 2 Sphere */


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
  vec3 estDiff = uniforms.cameraPos - WorldPos(estPos);
  return length(estDiff);
}

vec3 ModelPos(vec2 m_pos) {
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
  return vec3(pos.x, height, pos.y);
}

void main() {
  vTexCoord = vec2(0);
  vec3 worldPos = WorldPos(ModelPos(aPos));
  gl_Position = uniforms.mvp * vec4(worldPos.xyz, 1);

  // GL->VK conventions
  gl_Position.y = -gl_Position.y;
}
