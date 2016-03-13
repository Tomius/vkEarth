#ifndef GLSL_2_SPV_HPP_
#define GLSL_2_SPV_HPP_

#include <string>
#include <vector>
#include <vulkan/vk_cpp.h>

std::vector<unsigned int> GLSLtoSPV(const vk::ShaderStageFlagBits shaderType,
                                    const std::string& shaderText);

#endif // GLSL_2_SPV_HPP_
