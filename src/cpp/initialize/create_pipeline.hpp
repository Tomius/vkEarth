#ifndef CREATE_PIPELINE_HPP_
#define CREATE_PIPELINE_HPP_

#include <vulkan/vk_cpp.h>

namespace Initialize {

vk::Pipeline PreparePipeline(const vk::Device& device,
                             const vk::PipelineVertexInputStateCreateInfo& vertexState,
                             const vk::PipelineLayout& pipelineLayout,
                             const vk::RenderPass& renderPass);

}

#endif // CREATE_PIPELINE_HPP_
