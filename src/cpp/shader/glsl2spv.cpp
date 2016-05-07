#include "shader/glsl2spv.hpp"

#include <iostream>
#include <SPIRV/GlslangToSpv.h>

void Shader::InitializeGlslang() {
  glslang::InitializeProcess();
}

void Shader::FinalizeGlslang() {
  glslang::FinalizeProcess();
}

static void InitResources(TBuiltInResource& resources) {
  resources.maxLights = 32;
  resources.maxClipPlanes = 6;
  resources.maxTextureUnits = 32;
  resources.maxTextureCoords = 32;
  resources.maxVertexAttribs = 64;
  resources.maxVertexUniformComponents = 4096;
  resources.maxVaryingFloats = 64;
  resources.maxVertexTextureImageUnits = 32;
  resources.maxCombinedTextureImageUnits = 80;
  resources.maxTextureImageUnits = 32;
  resources.maxFragmentUniformComponents = 4096;
  resources.maxDrawBuffers = 32;
  resources.maxVertexUniformVectors = 128;
  resources.maxVaryingVectors = 8;
  resources.maxFragmentUniformVectors = 16;
  resources.maxVertexOutputVectors = 16;
  resources.maxFragmentInputVectors = 15;
  resources.minProgramTexelOffset = -8;
  resources.maxProgramTexelOffset = 7;
  resources.maxClipDistances = 8;
  resources.maxComputeWorkGroupCountX = 65535;
  resources.maxComputeWorkGroupCountY = 65535;
  resources.maxComputeWorkGroupCountZ = 65535;
  resources.maxComputeWorkGroupSizeX = 1024;
  resources.maxComputeWorkGroupSizeY = 1024;
  resources.maxComputeWorkGroupSizeZ = 64;
  resources.maxComputeUniformComponents = 1024;
  resources.maxComputeTextureImageUnits = 16;
  resources.maxComputeImageUniforms = 8;
  resources.maxComputeAtomicCounters = 8;
  resources.maxComputeAtomicCounterBuffers = 1;
  resources.maxVaryingComponents = 60;
  resources.maxVertexOutputComponents = 64;
  resources.maxGeometryInputComponents = 64;
  resources.maxGeometryOutputComponents = 128;
  resources.maxFragmentInputComponents = 128;
  resources.maxImageUnits = 8;
  resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
  resources.maxCombinedShaderOutputResources = 8;
  resources.maxImageSamples = 0;
  resources.maxVertexImageUniforms = 0;
  resources.maxTessControlImageUniforms = 0;
  resources.maxTessEvaluationImageUniforms = 0;
  resources.maxGeometryImageUniforms = 0;
  resources.maxFragmentImageUniforms = 8;
  resources.maxCombinedImageUniforms = 8;
  resources.maxGeometryTextureImageUnits = 16;
  resources.maxGeometryOutputVertices = 256;
  resources.maxGeometryTotalOutputComponents = 1024;
  resources.maxGeometryUniformComponents = 1024;
  resources.maxGeometryVaryingComponents = 64;
  resources.maxTessControlInputComponents = 128;
  resources.maxTessControlOutputComponents = 128;
  resources.maxTessControlTextureImageUnits = 16;
  resources.maxTessControlUniformComponents = 1024;
  resources.maxTessControlTotalOutputComponents = 4096;
  resources.maxTessEvaluationInputComponents = 128;
  resources.maxTessEvaluationOutputComponents = 128;
  resources.maxTessEvaluationTextureImageUnits = 16;
  resources.maxTessEvaluationUniformComponents = 1024;
  resources.maxTessPatchComponents = 120;
  resources.maxPatchVertices = 32;
  resources.maxTessGenLevel = 64;
  resources.maxViewports = 16;
  resources.maxVertexAtomicCounters = 0;
  resources.maxTessControlAtomicCounters = 0;
  resources.maxTessEvaluationAtomicCounters = 0;
  resources.maxGeometryAtomicCounters = 0;
  resources.maxFragmentAtomicCounters = 8;
  resources.maxCombinedAtomicCounters = 8;
  resources.maxAtomicCounterBindings = 1;
  resources.maxVertexAtomicCounterBuffers = 0;
  resources.maxTessControlAtomicCounterBuffers = 0;
  resources.maxTessEvaluationAtomicCounterBuffers = 0;
  resources.maxGeometryAtomicCounterBuffers = 0;
  resources.maxFragmentAtomicCounterBuffers = 1;
  resources.maxCombinedAtomicCounterBuffers = 1;
  resources.maxAtomicCounterBufferSize = 16384;
  resources.maxTransformFeedbackBuffers = 4;
  resources.maxTransformFeedbackInterleavedComponents = 64;
  resources.maxCullDistances = 8;
  resources.maxCombinedClipAndCullDistances = 8;
  resources.maxSamples = 4;
  resources.limits.nonInductiveForLoops = 1;
  resources.limits.whileLoops = 1;
  resources.limits.doWhileLoops = 1;
  resources.limits.generalUniformIndexing = 1;
  resources.limits.generalAttributeMatrixVectorIndexing = 1;
  resources.limits.generalVaryingIndexing = 1;
  resources.limits.generalSamplerIndexing = 1;
  resources.limits.generalVariableIndexing = 1;
  resources.limits.generalConstantMatrixVectorIndexing = 1;
}

static EShLanguage FindLanguage(const VkShaderStageFlagBits shader_type) {
  switch (shader_type) {
    case VK_SHADER_STAGE_VERTEX_BIT:
      return EShLangVertex;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
      return EShLangTessControl;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
      return EShLangTessEvaluation;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
      return EShLangGeometry;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
      return EShLangFragment;
    case VK_SHADER_STAGE_COMPUTE_BIT:
      return EShLangCompute;
    default:
      return EShLangVertex;
  }
}

static void TrimRight(std::string& str) {
  size_t endpos = str.find_last_not_of(" \t\n");
  if (std::string::npos != endpos) {
    str = str.substr(0, endpos+1);
  }
}

template <typename T>
static void PrintErrorLog(T& obj) {
  std::string info_log = obj.getInfoLog();
  TrimRight(info_log);
  bool show_info_log = !info_log.empty() &&
  info_log.rfind(" is not yet complete; most version-specific features are "
                 "present, but some are missing.") == info_log.npos;
  if (show_info_log) {
    std::cerr << info_log << std::endl;
  }

  std::string debug_info_log = obj.getInfoDebugLog();
  TrimRight(debug_info_log);
  if (!debug_info_log.empty()) {
    std::cerr << debug_info_log << std::endl;
  }
}

std::vector<unsigned int> Shader::GLSLtoSPV(const vk::ShaderStageFlagBits shaderType,
                                            const std::string& shaderText) {
  EShLanguage stage = FindLanguage(static_cast<VkShaderStageFlagBits>(shaderType));
  glslang::TShader shader(stage);
  glslang::TProgram program;

  const char *shaderStrings[1];
  TBuiltInResource resources;
  InitResources(resources);

  // Enable SPIR-V and Vulkan rules when parsing GLSL
  EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

  shaderStrings[0] = shaderText.c_str();
  shader.setStrings(shaderStrings, 1);

  bool parseSuccess = shader.parse(&resources, 100, false, messages);
  PrintErrorLog(shader); // parse success can still mean warnings
  if (!parseSuccess) {
    throw std::runtime_error(shader.getInfoLog());
  }

  program.addShader(&shader);
  bool linkSuccess = program.link(messages);
  if (!linkSuccess) {
    PrintErrorLog(program);
    throw std::runtime_error(shader.getInfoLog());
  }

  std::vector<unsigned int> spirv;
  glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);
  return spirv;
}
