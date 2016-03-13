#ifndef GLSL_2_SPV_HPP_
#define GLSL_2_SPV_HPP_

#include <stdexcept>
#include <SPIRV/GlslangToSpv.h>

void init_resources(TBuiltInResource &Resources) {
    Resources.maxLights = 32;
    Resources.maxClipPlanes = 6;
    Resources.maxTextureUnits = 32;
    Resources.maxTextureCoords = 32;
    Resources.maxVertexAttribs = 64;
    Resources.maxVertexUniformComponents = 4096;
    Resources.maxVaryingFloats = 64;
    Resources.maxVertexTextureImageUnits = 32;
    Resources.maxCombinedTextureImageUnits = 80;
    Resources.maxTextureImageUnits = 32;
    Resources.maxFragmentUniformComponents = 4096;
    Resources.maxDrawBuffers = 32;
    Resources.maxVertexUniformVectors = 128;
    Resources.maxVaryingVectors = 8;
    Resources.maxFragmentUniformVectors = 16;
    Resources.maxVertexOutputVectors = 16;
    Resources.maxFragmentInputVectors = 15;
    Resources.minProgramTexelOffset = -8;
    Resources.maxProgramTexelOffset = 7;
    Resources.maxClipDistances = 8;
    Resources.maxComputeWorkGroupCountX = 65535;
    Resources.maxComputeWorkGroupCountY = 65535;
    Resources.maxComputeWorkGroupCountZ = 65535;
    Resources.maxComputeWorkGroupSizeX = 1024;
    Resources.maxComputeWorkGroupSizeY = 1024;
    Resources.maxComputeWorkGroupSizeZ = 64;
    Resources.maxComputeUniformComponents = 1024;
    Resources.maxComputeTextureImageUnits = 16;
    Resources.maxComputeImageUniforms = 8;
    Resources.maxComputeAtomicCounters = 8;
    Resources.maxComputeAtomicCounterBuffers = 1;
    Resources.maxVaryingComponents = 60;
    Resources.maxVertexOutputComponents = 64;
    Resources.maxGeometryInputComponents = 64;
    Resources.maxGeometryOutputComponents = 128;
    Resources.maxFragmentInputComponents = 128;
    Resources.maxImageUnits = 8;
    Resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
    Resources.maxCombinedShaderOutputResources = 8;
    Resources.maxImageSamples = 0;
    Resources.maxVertexImageUniforms = 0;
    Resources.maxTessControlImageUniforms = 0;
    Resources.maxTessEvaluationImageUniforms = 0;
    Resources.maxGeometryImageUniforms = 0;
    Resources.maxFragmentImageUniforms = 8;
    Resources.maxCombinedImageUniforms = 8;
    Resources.maxGeometryTextureImageUnits = 16;
    Resources.maxGeometryOutputVertices = 256;
    Resources.maxGeometryTotalOutputComponents = 1024;
    Resources.maxGeometryUniformComponents = 1024;
    Resources.maxGeometryVaryingComponents = 64;
    Resources.maxTessControlInputComponents = 128;
    Resources.maxTessControlOutputComponents = 128;
    Resources.maxTessControlTextureImageUnits = 16;
    Resources.maxTessControlUniformComponents = 1024;
    Resources.maxTessControlTotalOutputComponents = 4096;
    Resources.maxTessEvaluationInputComponents = 128;
    Resources.maxTessEvaluationOutputComponents = 128;
    Resources.maxTessEvaluationTextureImageUnits = 16;
    Resources.maxTessEvaluationUniformComponents = 1024;
    Resources.maxTessPatchComponents = 120;
    Resources.maxPatchVertices = 32;
    Resources.maxTessGenLevel = 64;
    Resources.maxViewports = 16;
    Resources.maxVertexAtomicCounters = 0;
    Resources.maxTessControlAtomicCounters = 0;
    Resources.maxTessEvaluationAtomicCounters = 0;
    Resources.maxGeometryAtomicCounters = 0;
    Resources.maxFragmentAtomicCounters = 8;
    Resources.maxCombinedAtomicCounters = 8;
    Resources.maxAtomicCounterBindings = 1;
    Resources.maxVertexAtomicCounterBuffers = 0;
    Resources.maxTessControlAtomicCounterBuffers = 0;
    Resources.maxTessEvaluationAtomicCounterBuffers = 0;
    Resources.maxGeometryAtomicCounterBuffers = 0;
    Resources.maxFragmentAtomicCounterBuffers = 1;
    Resources.maxCombinedAtomicCounterBuffers = 1;
    Resources.maxAtomicCounterBufferSize = 16384;
    Resources.maxTransformFeedbackBuffers = 4;
    Resources.maxTransformFeedbackInterleavedComponents = 64;
    Resources.maxCullDistances = 8;
    Resources.maxCombinedClipAndCullDistances = 8;
    Resources.maxSamples = 4;
    Resources.limits.nonInductiveForLoops = 1;
    Resources.limits.whileLoops = 1;
    Resources.limits.doWhileLoops = 1;
    Resources.limits.generalUniformIndexing = 1;
    Resources.limits.generalAttributeMatrixVectorIndexing = 1;
    Resources.limits.generalVaryingIndexing = 1;
    Resources.limits.generalSamplerIndexing = 1;
    Resources.limits.generalVariableIndexing = 1;
    Resources.limits.generalConstantMatrixVectorIndexing = 1;
}

EShLanguage FindLanguage(const VkShaderStageFlagBits shader_type) {
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
void PrintErrorLog(T& obj) {
  std::string infoLog = obj.getInfoLog();
  TrimRight(infoLog);
  if (!infoLog.empty()) {
    std::cerr << infoLog << std::endl;
  }

  std::string debugInfoLog = obj.getInfoDebugLog();
  TrimRight(debugInfoLog);
  if (!debugInfoLog.empty()) {
    std::cerr << debugInfoLog << std::endl;
  }
}

void GLSLtoSPV(const VkShaderStageFlagBits shader_type, const char *pshader,
               std::vector<unsigned int> &spirv) {
    EShLanguage stage = FindLanguage(shader_type);
    glslang::TShader shader(stage);
    glslang::TProgram program;

    const char *shaderStrings[1];
    TBuiltInResource Resources;
    init_resources(Resources);

    // Enable SPIR-V and Vulkan rules when parsing GLSL
    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

    shaderStrings[0] = pshader;
    shader.setStrings(shaderStrings, 1);

    bool parseSuccess = shader.parse(&Resources, 100, false, messages);
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

    glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);
}

std::vector<unsigned int> GetVertexShaderCode(std::string const& str) {
  std::vector<unsigned int> spirv;
  GLSLtoSPV((VkShaderStageFlagBits)vk::ShaderStageFlagBits::eVertex,
            str.c_str(), spirv);
  return spirv;
}

std::vector<unsigned int> GetFragmentShaderCode(std::string const& str) {
  std::vector<unsigned int> spirv;
  GLSLtoSPV((VkShaderStageFlagBits)vk::ShaderStageFlagBits::eFragment,
            str.c_str(), spirv);
  return spirv;
}


#endif // GLSL_2_SPV_HPP_
