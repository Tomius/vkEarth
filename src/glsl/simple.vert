// Copyright (c) 2016, Tamas Csala

#version 420

// in variables and uniforms
layout (location = 0) in ivec2 aPos;
layout (location = 1) in vec4 aRenderData;

layout (location = 2) in uint aCurrentGeometryTextureId;
layout (location = 3) in vec3 aCurrentGeometryTexturePosAndSize;

layout (location = 4) in uint aNextGeometryTextureId;
layout (location = 5) in vec3 aNextGeometryTexturePosAndSize;

layout (location = 6) in uint aCurrentNormalTextureId;
layout (location = 7) in vec3 aCurrentNormalTexturePosAndSize;

layout (location = 8) in uint aNextNormalTextureId;
layout (location = 9) in vec3 aNextNormalTexturePosAndSize;

layout (location = 10) in uint aCurrentDiffuseTextureId;
layout (location = 11) in vec3 aCurrentDiffuseTexturePosAndSize;

layout (location = 12) in uint aNextDiffuseTextureId;
layout (location = 13) in vec3 aNextDiffuseTexturePosAndSize;

uniform sampler2D heightmap[2047];

layout (std140, binding = 1) uniform bufferVals {
  mat4 cameraMatrix;
  mat4 projectionMatrix;

  vec3 cameraPos;
  float depthCoef;

  float terrainSmallestGeometryLodDistance;
  float terrainSphereRadius;
  float faceSize;
  float heightScale;

  int terrainMaxLodLevel;
  int terrainLevelOffset;

  int textureDimension;
  int diffuseTextureDimensionWBorders;
  int elevationTextureDimensionWBorders;
} uniforms;

layout (location = 0) out vec3 vcPos;
layout (location = 1) out vec3 vwPos;
layout (location = 2) out vec3 vmPos;

layout (location = 3) out float vMorph;
layout (location = 4) out flat int vFace;

layout (location = 5) out flat uint vCurrentNormalTexId;
layout (location = 6) out flat uint vNextNormalTexId;
layout (location = 7) out flat vec3 vCurrentNormalTexPosAndSize;
layout (location = 8) out flat vec3 vNextNormalTexPosAndSize;

layout (location = 9) out flat uint vCurrentDiffuseTexId;
layout (location = 10) out flat uint vNextDiffuseTexId;
layout (location = 11) out flat vec3 vCurrentDiffuseTexPosAndSize;
layout (location = 12) out flat vec3 vNextDiffuseTexPosAndSize;

out gl_PerVertex {
  vec4 gl_Position;
};

// constants and aliases
const float kMorphEnd = 0.95, kMorphStart = 0.75;
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

float Sqr(float x) {
  return x * x;
}

vec3 Spherify(vec3 p) {
  return vec3(
    p.x * sqrt(1 - Sqr(p.y)/2 - Sqr(p.z)/2 + Sqr(p.y*p.z)/3),
    p.y * sqrt(1 - Sqr(p.z)/2 - Sqr(p.x)/2 + Sqr(p.z*p.x)/3),
    p.z * sqrt(1 - Sqr(p.x)/2 - Sqr(p.y)/2 + Sqr(p.x*p.y)/3)
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

vec4 cubic(float v) {
  vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
  vec4 s = n * n * n;
  float x = s.x;
  float y = s.y - 4.0 * s.x;
  float z = s.z - 4.0 * s.y + 6.0 * s.x;
  float w = 6.0 - x - y - z;
  return vec4(x, y, z, w) * (1.0/6.0);
}

vec4 textureBicubic(sampler2D tex, vec2 texCoords) {
  vec2 texSize = textureSize(tex, 0);
  vec2 invTexSize = 1.0 / texSize;

  texCoords = texCoords * texSize - 0.5;
  vec2 fxy = fract(texCoords);
  texCoords -= fxy;

  vec4 xcubic = cubic(fxy.x);
  vec4 ycubic = cubic(fxy.y);

  vec4 c = texCoords.xxyy + vec2(-0.5, +1.5).xyxy;

  vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
  vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;

  offset *= invTexSize.xxyy;

  vec4 sample0 = texture(tex, offset.xz);
  vec4 sample1 = texture(tex, offset.yz);
  vec4 sample2 = texture(tex, offset.xw);
  vec4 sample3 = texture(tex, offset.yw);

  float sx = s.x / (s.x + s.y);
  float sy = s.z / (s.z + s.w);

  return mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
}

vec2 Terrain_nodeLocal2Global(vec2 nodeCoord) {
  return terrainOffset + terrainScale * nodeCoord;
}

float GetHeightFast(vec2 pos) {
  uint texid = aCurrentGeometryTextureId;
  vec3 texPosAndSize = aCurrentGeometryTexturePosAndSize;
  vec2 samplePos = (pos - texPosAndSize.xy) / texPosAndSize.z;
  samplePos += 0.5 / uniforms.elevationTextureDimensionWBorders;
  float normalized_height = texture(heightmap[texid], samplePos).r;
  return normalized_height * uniforms.heightScale;
}

float GetHeightInternal(vec2 pos, uint texid, vec3 texPosAndSize) {
  vec2 samplePos = (pos - texPosAndSize.xy) / texPosAndSize.z;
  samplePos += 0.5 / uniforms.elevationTextureDimensionWBorders;
  float normalized_height = textureBicubic(heightmap[texid], samplePos).r;
  return normalized_height * uniforms.heightScale;
}

float GetHeight(vec2 pos, float morph) {
  float height0 =
    GetHeightInternal(pos, aCurrentGeometryTextureId,
                              aCurrentGeometryTexturePosAndSize);
  if (morph == 0.0 || terrainLevel < uniforms.terrainLevelOffset) {
    return height0;
  }

  float height1 =
    GetHeightInternal(pos, aNextGeometryTextureId,
                              aNextGeometryTexturePosAndSize);

  return mix(height0, height1, morph);
}

vec2 MorphVertex(vec2 vertex, float morph) {
  return vertex - fract(vertex * 0.5) * 2.0 * morph;
}

vec2 NodeLocal2Global(vec2 nodeCoord) {
  return terrainOffset + terrainScale * nodeCoord;
}

float EstimateDistance(vec2 geomPos) {
  float estHeight = GetHeightFast(geomPos);
  vec3 estPos = vec3(geomPos.x, estHeight, geomPos.y);
  vec3 estDiff = uniforms.cameraPos - WorldPos(estPos);
  return length(estDiff);
}

vec4 ModelPos(vec2 m_pos) {
  vec2 pos = NodeLocal2Global(m_pos);
  float dist = EstimateDistance(pos);
  float morph = 0;

  if (terrainLevel < uniforms.terrainMaxLodLevel) {
    float nextLevelSize = 2 * terrainScale * uniforms.terrainSmallestGeometryLodDistance;
    float maxDist = kMorphEnd * nextLevelSize;
    float startDist = kMorphStart * nextLevelSize;
    morph = smoothstep(startDist, maxDist, dist);

    vec2 morphed_pos = MorphVertex(m_pos, morph);
    pos = NodeLocal2Global(morphed_pos);
  }

  float height = GetHeight(pos, morph);
  return vec4(pos.x, height, pos.y, morph);
}

void main() {
  vec4 modelPos = ModelPos(aPos);
  vmPos = modelPos.xyz;
  vMorph = modelPos.w;

  vec3 worldPos = WorldPos(modelPos.xyz);
  vwPos = worldPos;

  vec3 cameraPos = (uniforms.cameraMatrix * vec4(worldPos, 1)).xyz;
  vcPos = cameraPos;

  gl_Position = uniforms.projectionMatrix * vec4(cameraPos, 1);

  // GL->VK convention
  gl_Position.y = -gl_Position.y;

  vFace = terrainFace;

  vCurrentNormalTexId = aCurrentNormalTextureId;
  vCurrentNormalTexPosAndSize = aCurrentNormalTexturePosAndSize;
  vNextNormalTexId = aNextNormalTextureId;
  vNextNormalTexPosAndSize = aNextNormalTexturePosAndSize;

  vCurrentDiffuseTexId = aCurrentDiffuseTextureId;
  vCurrentDiffuseTexPosAndSize = aCurrentDiffuseTexturePosAndSize;
  vNextDiffuseTexId = aNextDiffuseTextureId;
  vNextDiffuseTexPosAndSize = aNextDiffuseTexturePosAndSize;
}
