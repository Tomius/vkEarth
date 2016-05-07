// Copyright (c) 2016, Tamas Csala

#version 420

layout (location = 0) in vec3 vcPos;
layout (location = 1) in vec3 vwPos;
layout (location = 2) in vec3 vmPos;

layout (location = 3) in float vMorph;
layout (location = 4) in flat int vFace;

layout (location = 5) in flat uint vCurrentNormalTexId;
layout (location = 6) in flat uint vNextNormalTexId;
layout (location = 7) in flat vec3 vCurrentNormalTexPosAndSize;
layout (location = 8) in flat vec3 vNextNormalTexPosAndSize;

layout (location = 9) in flat uint vCurrentDiffuseTexId;
layout (location = 10) in flat uint vNextDiffuseTexId;
layout (location = 11) in flat vec3 vCurrentDiffuseTexPosAndSize;
layout (location = 12) in flat vec3 vNextDiffuseTexPosAndSize;

layout (location = 13) in vec2 vTexCoord;

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

layout (location = 0) out vec4 outColor;

const float kMorphEnd = 0.95, kMorphStart = 0.65;

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
  switch (vFace) {
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

float GetHeight(vec2 pos, uint tex_id, vec3 tex_pos_and_size) {
  vec2 samplePos = (pos - tex_pos_and_size.xy) / tex_pos_and_size.z;
  samplePos += 0.5 / uniforms.elevationTextureDimensionWBorders;
  return textureBicubic(heightmap[tex_id], samplePos).r * uniforms.heightScale;
}

vec3 GetNormalModelSpaceInternal(vec2 pos, uint tex_id, vec3 tex_pos_and_size) {
  float diff = tex_pos_and_size.z / uniforms.elevationTextureDimensionWBorders;
  float py = GetHeight(vec2(pos.x, pos.y + diff), tex_id, tex_pos_and_size);
  float my = GetHeight(vec2(pos.x, pos.y - diff), tex_id, tex_pos_and_size);
  float px = GetHeight(vec2(pos.x + diff, pos.y), tex_id, tex_pos_and_size);
  float mx = GetHeight(vec2(pos.x - diff, pos.y), tex_id, tex_pos_and_size);

  return normalize(vec3(mx-px, diff, my-py));
}

vec3 GetNormalModelSpace(vec2 pos) {
  float dist = length(vcPos);
  float next_dist = vNextNormalTexPosAndSize.z
      / uniforms.textureDimension * uniforms.terrainSmallestGeometryLodDistance;
  float morph = smoothstep(kMorphStart*next_dist, kMorphEnd*next_dist, dist);
  vec3 normal0 = GetNormalModelSpaceInternal(pos, vCurrentNormalTexId,
                                             vCurrentNormalTexPosAndSize);
  if (morph == 0.0) {
    return normal0;
  }

  vec3 normal1 = GetNormalModelSpaceInternal(pos, vNextNormalTexId,
                                             vNextNormalTexPosAndSize);

  return mix(normal0, normal1, morph);
}

vec3 GetNormal(vec2 pos) {
  return WorldPos(vmPos + GetNormalModelSpace(pos)) - WorldPos(vmPos);
}

// Color

// todo remove code duplication
vec3 GetColor(vec2 pos, uint tex_id, vec3 tex_pos_and_size) {
  vec2 samplePos = (pos - tex_pos_and_size.xy) / tex_pos_and_size.z;
  samplePos += 0.5 / uniforms.diffuseTextureDimensionWBorders;
  return textureBicubic(heightmap[tex_id], samplePos).rgb;
}

vec3 GetDiffuseColor(vec2 pos) {
  float dist = length(vcPos);
  float next_dist = vNextDiffuseTexPosAndSize.z
      / uniforms.textureDimension * uniforms.terrainSmallestGeometryLodDistance;
  float morph = smoothstep(kMorphStart*next_dist, kMorphEnd*next_dist, dist);
  vec3 diffuse0 = GetColor(pos, vCurrentDiffuseTexId,
                           vCurrentDiffuseTexPosAndSize);
  if (morph == 0.0) {
    return diffuse0;
  }

  vec3 diffuse1 = GetColor(pos, vNextDiffuseTexId,
                           vNextDiffuseTexPosAndSize);

  return mix(diffuse0, diffuse1, morph);
}

void main() {
  // outColor = vec4(1.0);
  float lighting = dot(GetNormal(vmPos.xz), normalize(vec3(1, 1, 1)));
  float luminance = 0.2 + 0.8*max(lighting, 0.0) + 0.1 * (1+lighting)/2;
  vec3 diffuse = GetDiffuseColor(vmPos.xz);

  outColor = vec4(luminance*diffuse, 1);
}
