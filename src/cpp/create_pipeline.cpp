#include "create_pipeline.hpp"

#include <vector>
#include <cassert>
#include <iostream>

#include "file_utils.hpp"
#include "glsl2spv.hpp"
#include "error_checking.hpp"

static vk::ShaderModule PrepareShaderModule(vk::Device& device,
                                                   const void *code,
                                                   size_t size) {
  vk::ShaderModuleCreateInfo moduleCreateInfo;
  vk::ShaderModule module;

  moduleCreateInfo.codeSize(size);
  moduleCreateInfo.pCode(static_cast<const uint32_t*>(code));
  vk::chk(device.createShaderModule(&moduleCreateInfo, nullptr, &module));

  return module;
}

static vk::ShaderModule PrepareVs(vk::Device& device) {
  std::vector<unsigned int> vertShader =
    GLSLtoSPV(vk::ShaderStageFlagBits::eVertex,
              FileUtils::ReadFileToString("src/glsl/simple.vert"));

  return PrepareShaderModule(device, (const void*)vertShader.data(),
                             vertShader.size() * sizeof(vertShader[0]));
}

static vk::ShaderModule PrepareFs(vk::Device& device) {
  std::vector<unsigned int> fragShader =
    GLSLtoSPV(vk::ShaderStageFlagBits::eFragment,
              FileUtils::ReadFileToString("src/glsl/simple.frag"));

  return PrepareShaderModule(device, (const void*)fragShader.data(),
                             fragShader.size() * sizeof(fragShader[0]));
}


vk::Pipeline PreparePipeline(vk::Device& device,
                             const vk::PipelineVertexInputStateCreateInfo& vertexState,
                             const vk::PipelineLayout& pipelineLayout,
                             const vk::RenderPass& renderPass) {
  vk::GraphicsPipelineCreateInfo pipelineCreateInfo;

  vk::PipelineInputAssemblyStateCreateInfo ia;
  vk::PipelineRasterizationStateCreateInfo rs;
  vk::PipelineColorBlendStateCreateInfo cb;
  vk::PipelineDepthStencilStateCreateInfo ds;
  vk::PipelineViewportStateCreateInfo vp;
  vk::PipelineMultisampleStateCreateInfo ms;
  vk::DynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE];
  vk::PipelineDynamicStateCreateInfo dynamicState;

  dynamicState.pDynamicStates(dynamicStateEnables);
  pipelineCreateInfo.layout(pipelineLayout);
  ia.topology(vk::PrimitiveTopology::eTriangleList);

  rs.polygonMode(vk::PolygonMode::eFill);
  rs.cullMode(vk::CullModeFlagBits::eBack);
  rs.frontFace(vk::FrontFace::eClockwise);
  rs.depthClampEnable(VK_FALSE);
  rs.rasterizerDiscardEnable(VK_FALSE);
  rs.depthBiasEnable(VK_FALSE);

  vk::PipelineColorBlendAttachmentState att_state[1];
  att_state[0].colorWriteMask(vk::ColorComponentFlagBits::eR |
                              vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB |
                              vk::ColorComponentFlagBits::eA);
  att_state[0].blendEnable(VK_FALSE);
  cb.attachmentCount(1);
  cb.pAttachments(att_state);

  vp.viewportCount(1);
  dynamicStateEnables[dynamicState.dynamicStateCount()] =
      vk::DynamicState::eViewport;
  dynamicState.dynamicStateCount(dynamicState.dynamicStateCount() + 1);

  vp.scissorCount(1);
  dynamicStateEnables[dynamicState.dynamicStateCount()] =
      vk::DynamicState::eScissor;
  dynamicState.dynamicStateCount(dynamicState.dynamicStateCount() + 1);

  ds.depthTestEnable(VK_TRUE);
  ds.depthWriteEnable(VK_TRUE);
  ds.depthCompareOp(vk::CompareOp::eLessOrEqual);
  ds.depthBoundsTestEnable(VK_FALSE);
  ds.stencilTestEnable(VK_FALSE);

  vk::StencilOpState stencilState = vk::StencilOpState()
    .failOp(vk::StencilOp::eKeep)
    .passOp(vk::StencilOp::eKeep)
    .compareOp(vk::CompareOp::eAlways);
  ds.back(stencilState);
  ds.front(stencilState);

  ms.pSampleMask(nullptr);
  ms.rasterizationSamples(vk::SampleCountFlagBits::e1);

  // Two stages: vs and fs
  pipelineCreateInfo.stageCount(2);
  vk::PipelineShaderStageCreateInfo shaderStages[2];

  shaderStages[0].stage(vk::ShaderStageFlagBits::eVertex);
  shaderStages[0].module(PrepareVs(device));
  shaderStages[0].pName("main");

  shaderStages[1].stage(vk::ShaderStageFlagBits::eFragment);
  shaderStages[1].module(PrepareFs(device));
  shaderStages[1].pName("main");

  pipelineCreateInfo.pVertexInputState(&vertexState);
  pipelineCreateInfo.pInputAssemblyState(&ia);
  pipelineCreateInfo.pRasterizationState(&rs);
  pipelineCreateInfo.pColorBlendState(&cb);
  pipelineCreateInfo.pMultisampleState(&ms);
  pipelineCreateInfo.pViewportState(&vp);
  pipelineCreateInfo.pDepthStencilState(&ds);
  pipelineCreateInfo.pStages(shaderStages);
  pipelineCreateInfo.renderPass(renderPass);
  pipelineCreateInfo.pDynamicState(&dynamicState);

  vk::PipelineCache pipelineCache;
  vk::PipelineCacheCreateInfo pipelineCacheCreateInfo;
  vk::chk(device.createPipelineCache(&pipelineCacheCreateInfo, nullptr,
                                     &pipelineCache));

  vk::Pipeline pipeline;
  vk::chk(device.createGraphicsPipelines(pipelineCache, 1, &pipelineCreateInfo,
                                         nullptr, &pipeline));

  device.destroyPipelineCache(pipelineCache, nullptr);

  device.destroyShaderModule(shaderStages[0].module(), nullptr);
  device.destroyShaderModule(shaderStages[1].module(), nullptr);

  return pipeline;
}
