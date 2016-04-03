// Copyright (c) 2016, Tamas Csala

#include "demo_scene.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdbool>
#include <cassert>
#include <iostream>
#include <vector>
#include <memory>

#include <vulkan/vk_cpp.h>
#include <GLFW/glfw3.h>
#include <lodepng.h>

#include "engine/scene.hpp"
#include "common/error_checking.hpp"
#include "initialize/create_pipeline.hpp"
#include "common/vulkan_memory.hpp"

#define VERTEX_BUFFER_BIND_ID 0
#define INSTANCE_BUFFER_BIND_ID 1

void DemoScene::BuildDrawCmd() {
  const vk::CommandBufferInheritanceInfo cmd_buf_hinfo;
  const vk::CommandBufferBeginInfo cmd_buf_info =
      vk::CommandBufferBeginInfo().pInheritanceInfo(&cmd_buf_hinfo);
  const vk::ClearValue clear_values[2] = {
      vk::ClearValue().color(
          vk::ClearColorValue{std::array<float,4>{{0.2f, 0.2f, 0.2f, 0.2f}}}),
      vk::ClearValue().depthStencil(vk::ClearDepthStencilValue{1.0, 0})
  };
  const vk::RenderPassBeginInfo rp_begin = vk::RenderPassBeginInfo()
      .renderPass(render_pass_)
      .framebuffer(framebuffers_.get()[vkCurrentBuffer()])
      .renderArea({vk::Offset2D(0, 0),
          vk::Extent2D(framebufferSize().x, framebufferSize().y)})
      .clearValueCount(2)
      .pClearValues(clear_values);

  vk::chk(vkDrawCmd().begin(&cmd_buf_info));

  vkDrawCmd().beginRenderPass(&rp_begin, vk::SubpassContents::eInline);
  vkDrawCmd().bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_);
  vkDrawCmd().bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
      pipeline_layout_, 0, 1, &desc_set_, 0, nullptr);

  vk::Viewport viewport = vk::Viewport()
    .width(framebufferSize().x)
    .height(framebufferSize().y)
    .minDepth(0.0f)
    .maxDepth(1.0f);
  vkDrawCmd().setViewport(0, 1, &viewport);

  vk::Rect2D scissor{vk::Offset2D(0, 0), vk::Extent2D(framebufferSize().x, framebufferSize().y)};
  vkDrawCmd().setScissor(0, 1, &scissor);

  vk::DeviceSize offsets[1] = {0};
  vkDrawCmd().bindVertexBuffers(VERTEX_BUFFER_BIND_ID, 1,
                                &vertex_attribs_.buf, offsets);
  vkDrawCmd().bindVertexBuffers(INSTANCE_BUFFER_BIND_ID, 1,
                                &instance_attribs_.buf, offsets);
  vkDrawCmd().bindIndexBuffer(indices_.buf,
                              vk::DeviceSize{},
                              vk::IndexType::eUint16);

  vkDrawCmd().setLineWidth(2.0f);

  vkDrawCmd().drawIndexed(grid_mesh_.mesh_.index_count_,
                          grid_mesh_.mesh_.render_data_.size(), 0, 0, 0);
  vkDrawCmd().endRenderPass();

  vk::ImageMemoryBarrier pre_present_barrier = vk::ImageMemoryBarrier()
      .srcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
      .dstAccessMask(vk::AccessFlagBits::eMemoryRead)
      .oldLayout(vk::ImageLayout::eColorAttachmentOptimal)
      .newLayout(vk::ImageLayout::ePresentSrcKHR)
      .srcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
      .dstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
      .subresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

  pre_present_barrier.image(vkBuffers()[vkCurrentBuffer()].image);
  vkDrawCmd().pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                              vk::PipelineStageFlagBits::eBottomOfPipe,
                              vk::DependencyFlags(), 0, nullptr, 0,
                              nullptr, 1, &pre_present_barrier);

  vk::chk(vkDrawCmd().end());
}

void DemoScene::Draw() {
  vk::Semaphore present_complete_semaphore;
  vk::SemaphoreCreateInfo present_complete_semaphore_create_info;

  vk::chk(vkDevice().createSemaphore(&present_complete_semaphore_create_info,
                                     nullptr, &present_complete_semaphore));

  // Get the index of the next available swapchain image:
  VkResult vkErr = vkApp().entry_points.AcquireNextImageKHR(
      vkDevice(), vkSwapchain(), UINT64_MAX, present_complete_semaphore,
      (vk::Fence)nullptr, &vkCurrentBuffer());
  if (vkErr == VK_ERROR_OUT_OF_DATE_KHR) {
      // vkSwapchain() is out of date (e.g. the window was resized) and
      // must be recreated:
      //demo_resize(demo, scene);
      throw std::runtime_error("Should resize swapchain");
      Draw();
      vkDevice().destroySemaphore(present_complete_semaphore, nullptr);
      return;
  } else if (vkErr == VK_SUBOPTIMAL_KHR) {
      // vkSwapchain() is not as optimal as it could be, but the platform's
      // presentation engine will still present the image correctly.
  } else {
      assert(vkErr == VK_SUCCESS);
  }

  // Assume the command buffer has been run on current_buffer before so
  // we need to set the image layout back to COLOR_ATTACHMENT_OPTIMAL
  SetImageLayout(vkBuffers()[vkCurrentBuffer()].image,
                 vk::ImageAspectFlagBits::eColor,
                 vk::ImageLayout::ePresentSrcKHR,
                 vk::ImageLayout::eColorAttachmentOptimal,
                 vk::AccessFlags{});
  FlushInitCommand();

  // Wait for the present complete semaphore to be signaled to ensure
  // that the image won't be rendered to until the presentation
  // engine has fully released ownership to the application, and it is
  // okay to render to the image.

  // FIXME/TODO: DEAL WITH vk::ImageLayout::ePresentSrcKHR
  BuildDrawCmd();
  vk::Fence null_fence;
  vk::PipelineStageFlags pipe_stage_flags =
      vk::PipelineStageFlagBits::eBottomOfPipe;
  vk::SubmitInfo submit_info = vk::SubmitInfo()
      .waitSemaphoreCount(1)
      .pWaitSemaphores(&present_complete_semaphore)
      .pWaitDstStageMask(&pipe_stage_flags)
      .commandBufferCount(1)
      .pCommandBuffers(&vkDrawCmd());

  vk::chk(vkQueue().submit(1, &submit_info, null_fence));

  vk::PresentInfoKHR present = vk::PresentInfoKHR()
      .swapchainCount(1)
      .pSwapchains(&vkSwapchain())
      .pImageIndices(&vkCurrentBuffer());
  VkPresentInfoKHR& vkPresent =
    const_cast<VkPresentInfoKHR&>(
      static_cast<const VkPresentInfoKHR&>(present));

  // TBD/TODO: SHOULD THE "present" PARAMETER BE "const" IN THE HEADER?
  vkErr = vkApp().entry_points.QueuePresentKHR(vkQueue(), &vkPresent);
  if (vkErr == VK_ERROR_OUT_OF_DATE_KHR) {
      // vkSwapchain() is out of date (e.g. the window was resized) and
      // must be recreated:
      //demo_resize(demo, scene);
      throw std::runtime_error("Should resize swapchain");
  } else if (vkErr == VK_SUBOPTIMAL_KHR) {
      // vkSwapchain() is not as optimal as it could be, but the platform's
      // presentation engine will still present the image correctly.
  } else {
      assert(vkErr == VK_SUCCESS);
  }

  vk::chk(vkQueue().waitIdle());

  vkDevice().destroySemaphore(present_complete_semaphore, nullptr);
}

void DemoScene::PrepareTextureImage(const uint32_t *tex_colors,
                                    int32_t tex_width, int32_t tex_height,
                                    TextureObject *tex_obj, vk::ImageTiling tiling,
                                    vk::ImageUsageFlags usage,
                                    vk::MemoryPropertyFlags required_props,
                                    vk::Format tex_format) {
  tex_obj->tex_width = tex_width;
  tex_obj->tex_height = tex_height;

  const vk::ImageCreateInfo image_create_info = vk::ImageCreateInfo()
      .imageType(vk::ImageType::e2D)
      .format(tex_format)
      .extent(vk::Extent3D(tex_width, tex_height, 1))
      .mipLevels(1)
      .arrayLayers(1)
      .samples(vk::SampleCountFlagBits::e1)
      .tiling(tiling)
      .usage(usage)
      .initialLayout(vk::ImageLayout::ePreinitialized);

  vk::MemoryAllocateInfo mem_alloc;
  vk::MemoryRequirements mem_reqs;

  vk::chk(vkDevice().createImage(&image_create_info, nullptr, &tex_obj->image));

  vkDevice().getImageMemoryRequirements(tex_obj->image, &mem_reqs);

  mem_alloc.allocationSize(mem_reqs.size());
  MemoryTypeFromProperties(vkGpuMemoryProperties(),
                           mem_reqs.memoryTypeBits(),
                           required_props, mem_alloc);

  /* allocate memory */
  vk::chk(vkDevice().allocateMemory(&mem_alloc, nullptr, &tex_obj->mem));

  /* bind memory */
  vk::chk(vkDevice().bindImageMemory(tex_obj->image, tex_obj->mem, 0));

  if (required_props & vk::MemoryPropertyFlagBits::eHostVisible) {
      const vk::ImageSubresource subres =
        vk::ImageSubresource().aspectMask(vk::ImageAspectFlagBits::eColor);
      vk::SubresourceLayout layout;
      void *data;

      vkDevice().getImageSubresourceLayout(tex_obj->image, &subres, &layout);

      vk::chk(vkDevice().mapMemory(tex_obj->mem, 0, mem_alloc.allocationSize(),
                                   vk::MemoryMapFlags{}, &data));

      for (int32_t y = 0; y < tex_height; y++) {
          uint32_t *row = (uint32_t *)((char *)data + layout.rowPitch() * y);
          for (int32_t x = 0; x < tex_width; x++)
              row[x] = tex_colors[(x & 1) ^ (y & 1)];
      }

      vkDevice().unmapMemory(tex_obj->mem);
  }

  tex_obj->imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  SetImageLayout(tex_obj->image, vk::ImageAspectFlagBits::eColor,
                 vk::ImageLayout::eUndefined, tex_obj->imageLayout,
                 vk::AccessFlags{});
  /* setting the image layout does not reference the actual memory so no need
   * to add a mem ref */
}

void DemoScene::PrepareTextures() {
    std::vector<unsigned char> image; //the raw pixels
    unsigned width, height;
    unsigned error = lodepng::decode(image, width, height, "src/resources/gmted2010/0.png", LCT_RGBA, 16);
    if (error) {
      std::cerr << "image decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
      std::terminate();
    }

    vk::FormatProperties props;
    const vk::Format tex_format = vk::Format::eR16G16B16A16Unorm;
    vkGpu().getFormatProperties(tex_format, &props);

    for (uint32_t i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        if ((props.linearTilingFeatures() &
             vk::FormatFeatureFlagBits::eSampledImage) &&
            !kUseStagingBuffer) {
            /* Device can texture using linear textures */
            PrepareTextureImage((uint32_t *)image.data(),
                                width, height, &textures_[i],
                                vk::ImageTiling::eLinear,
                                vk::ImageUsageFlagBits::eSampled,
                                vk::MemoryPropertyFlagBits::eHostVisible,
                                tex_format);
        } else if (props.optimalTilingFeatures() &
                   vk::FormatFeatureFlagBits::eSampledImage) {
            /* Must use staging buffer to copy linear texture to optimized */
            TextureObject staging_texture;

            PrepareTextureImage((uint32_t *)image.data(),
                                 width, height, &staging_texture,
                                 vk::ImageTiling::eLinear,
                                 vk::ImageUsageFlagBits::eTransferSrc,
                                 vk::MemoryPropertyFlagBits::eHostVisible,
                                 tex_format);

            PrepareTextureImage((uint32_t *)image.data(),
                width, height, &textures_[i],
                vk::ImageTiling::eOptimal,
                (vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled),
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                tex_format);

            SetImageLayout(staging_texture.image,
                           vk::ImageAspectFlagBits::eColor,
                           staging_texture.imageLayout,
                           vk::ImageLayout::eTransferSrcOptimal,
                           vk::AccessFlagBits::eShaderRead);

            SetImageLayout(textures_[i].image,
                           vk::ImageAspectFlagBits::eColor,
                           textures_[i].imageLayout,
                           vk::ImageLayout::eTransferDstOptimal,
                           vk::AccessFlagBits::eShaderRead);

            vk::ImageCopy copy_region = vk::ImageCopy()
                .srcSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
                .dstSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
                .extent(vk::Extent3D(staging_texture.tex_width,
                                     staging_texture.tex_height, 1));
            vkSetupCmd().copyImage(staging_texture.image,
                vk::ImageLayout::eTransferSrcOptimal, textures_[i].image,
                vk::ImageLayout::eTransferDstOptimal, 1, &copy_region);

            SetImageLayout(textures_[i].image,
                           vk::ImageAspectFlagBits::eColor,
                           vk::ImageLayout::eTransferDstOptimal,
                           textures_[i].imageLayout,
                           vk::AccessFlagBits::eTransferWrite);

            FlushInitCommand();

            vkDevice().destroyImage(staging_texture.image, nullptr);
            vkDevice().freeMemory(staging_texture.mem, nullptr);
        } else {
            /* Can't support vk::Format::eB8G8R8A8Unorm !? */
            assert(!"No support for B8G8R8A8_UNORM as texture image format");
        }

        const vk::SamplerCreateInfo sampler = vk::SamplerCreateInfo()
            .magFilter(vk::Filter::eNearest)
            .minFilter(vk::Filter::eNearest)
            .mipmapMode(vk::SamplerMipmapMode::eNearest)
            .addressModeU(vk::SamplerAddressMode::eRepeat)
            .addressModeV(vk::SamplerAddressMode::eRepeat)
            .addressModeW(vk::SamplerAddressMode::eRepeat)
            .mipLodBias(0.0f)
            .anisotropyEnable(VK_FALSE)
            .maxAnisotropy(1)
            .compareOp(vk::CompareOp::eNever)
            .minLod(0.0f)
            .maxLod(0.0f)
            .borderColor(vk::BorderColor::eFloatOpaqueWhite)
            .unnormalizedCoordinates(VK_FALSE);

        vk::ImageViewCreateInfo view = vk::ImageViewCreateInfo()
            .viewType(vk::ImageViewType::e2D)
            .format(tex_format)
            .subresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

        /* create sampler */
        vk::chk(vkDevice().createSampler(&sampler, nullptr,
                                         &textures_[i].sampler));

        /* create image view */
        view.image(textures_[i].image);
        vk::chk(vkDevice().createImageView(&view, nullptr, &textures_[i].view));
    }
}

void DemoScene::PrepareIndices() {
  const std::vector<uint16_t>& indices = grid_mesh_.mesh_.indices_;
  const vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
      .size(sizeof(uint16_t) * indices.size())
      .usage(vk::BufferUsageFlagBits::eIndexBuffer);

  vk::MemoryAllocateInfo mem_alloc;
  vk::MemoryRequirements mem_reqs;
  void *data;

  vk::chk(vkDevice().createBuffer(&buf_info, nullptr, &indices_.buf));

  vkDevice().getBufferMemoryRequirements(indices_.buf, &mem_reqs);

  mem_alloc.allocationSize(mem_reqs.size());
  MemoryTypeFromProperties(vkGpuMemoryProperties(),
                           mem_reqs.memoryTypeBits(),
                           vk::MemoryPropertyFlagBits::eHostVisible,
                           mem_alloc);

  vk::chk(vkDevice().allocateMemory(&mem_alloc, nullptr, &indices_.mem));

  vk::chk(vkDevice().mapMemory(indices_.mem, 0, mem_alloc.allocationSize(),
                               vk::MemoryMapFlags{}, &data));

  std::memcpy(data, indices.data(), sizeof(uint16_t) * indices.size());

  vkDevice().unmapMemory(indices_.mem);

  vk::chk(vkDevice().bindBufferMemory(indices_.buf, indices_.mem, 0));
}

void DemoScene::PrepareVertices() {
  vk::MemoryAllocateInfo mem_alloc;
  vk::MemoryRequirements mem_reqs;

  { // vertexAttribs
    const vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
      .size(sizeof(svec2) * grid_mesh_.mesh_.positions_.size())
      .usage(vk::BufferUsageFlagBits::eVertexBuffer);

    vk::chk(vkDevice().createBuffer(&buf_info, nullptr, &vertex_attribs_.buf));

    vkDevice().getBufferMemoryRequirements(vertex_attribs_.buf, &mem_reqs);

    mem_alloc.allocationSize(mem_reqs.size());
    MemoryTypeFromProperties(vkGpuMemoryProperties(),
                             mem_reqs.memoryTypeBits(),
                             vk::MemoryPropertyFlagBits::eHostVisible,
                             mem_alloc);

    vk::chk(vkDevice().allocateMemory(&mem_alloc, nullptr, &vertex_attribs_.mem));
    vk::chk(vkDevice().bindBufferMemory(vertex_attribs_.buf, vertex_attribs_.mem, 0));

    void *data;
    vk::chk(vkDevice().mapMemory(vertex_attribs_.mem, 0, mem_alloc.allocationSize(),
                                 vk::MemoryMapFlags{}, &data));

    std::memcpy(data, grid_mesh_.mesh_.positions_.data(),
                sizeof(svec2) * grid_mesh_.mesh_.positions_.size());

    vkDevice().unmapMemory(vertex_attribs_.mem);
  }

  { // instanceAttribs
    const vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
      .size(sizeof(glm::vec4) * Settings::kMaxInstanceCount)
      .usage(vk::BufferUsageFlagBits::eVertexBuffer);

    vk::chk(vkDevice().createBuffer(&buf_info, nullptr, &instance_attribs_.buf));

    vkDevice().getBufferMemoryRequirements(instance_attribs_.buf, &mem_reqs);

    mem_alloc.allocationSize(mem_reqs.size());
    MemoryTypeFromProperties(vkGpuMemoryProperties(),
                             mem_reqs.memoryTypeBits(),
                             vk::MemoryPropertyFlagBits::eHostVisible,
                             mem_alloc);

    vk::chk(vkDevice().allocateMemory(&mem_alloc, nullptr, &instance_attribs_.mem));
    vk::chk(vkDevice().bindBufferMemory(instance_attribs_.buf, instance_attribs_.mem, 0));
  }

  vertex_input_.vertexBindingDescriptionCount(2);
  vertex_input_.pVertexBindingDescriptions(vertex_input_bindings_);
  vertex_input_.vertexAttributeDescriptionCount(2);
  vertex_input_.pVertexAttributeDescriptions(vertex_input_attribs_);

  vertex_input_bindings_[0].binding(VERTEX_BUFFER_BIND_ID);
  vertex_input_bindings_[0].stride(sizeof(svec2));
  vertex_input_bindings_[0].inputRate(vk::VertexInputRate::eVertex);

  vertex_input_attribs_[0].binding(VERTEX_BUFFER_BIND_ID);
  vertex_input_attribs_[0].location(0);
  vertex_input_attribs_[0].format(vk::Format::eR16G16Sint);
  vertex_input_attribs_[0].offset(0);

  vertex_input_bindings_[1].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_bindings_[1].stride(sizeof(glm::vec4));
  vertex_input_bindings_[1].inputRate(vk::VertexInputRate::eInstance);

  vertex_input_attribs_[1].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[1].location(1);
  vertex_input_attribs_[1].format(vk::Format::eR32G32B32A32Sfloat);
  vertex_input_attribs_[1].offset(0);
}

void DemoScene::PrepareDescriptorLayout() {
  const vk::DescriptorSetLayoutBinding layout_binding[] = {
    vk::DescriptorSetLayoutBinding()
      .binding(0)
      .descriptorType(vk::DescriptorType::eCombinedImageSampler)
      .descriptorCount(DEMO_TEXTURE_COUNT)
      .stageFlags(vk::ShaderStageFlagBits::eFragment),
    vk::DescriptorSetLayoutBinding()
      .binding(1)
      .descriptorType(vk::DescriptorType::eUniformBuffer)
      .descriptorCount(1)
      .stageFlags(vk::ShaderStageFlagBits::eVertex)
  };

  const vk::DescriptorSetLayoutCreateInfo descriptor_layout =
    vk::DescriptorSetLayoutCreateInfo()
      .bindingCount(2)
      .pBindings(layout_binding);

  vk::chk(vkDevice().createDescriptorSetLayout(&descriptor_layout, nullptr,
                                               &desc_layout_));

  const vk::PipelineLayoutCreateInfo pipeline_layout_create_info =
    vk::PipelineLayoutCreateInfo()
      .setLayoutCount(1)
      .pSetLayouts(&desc_layout_);

  vk::chk(vkDevice().createPipelineLayout(&pipeline_layout_create_info,
                                          nullptr, &pipeline_layout_));
}

void DemoScene::PrepareRenderPass() {
  const vk::AttachmentDescription attachments[2] = {
    vk::AttachmentDescription()
      .format(vkSurfaceFormat())
      .samples(vk::SampleCountFlagBits::e1)
      .loadOp(vk::AttachmentLoadOp::eClear)
      .storeOp(vk::AttachmentStoreOp::eStore)
      .stencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .stencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .initialLayout(vk::ImageLayout::eColorAttachmentOptimal)
      .finalLayout(vk::ImageLayout::eColorAttachmentOptimal),
    vk::AttachmentDescription()
      .format(vkDepthBuffer().format)
      .samples(vk::SampleCountFlagBits::e1)
      .loadOp(vk::AttachmentLoadOp::eClear)
      .storeOp(vk::AttachmentStoreOp::eDontCare)
      .stencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .stencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .initialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
      .finalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
  };
  const vk::AttachmentReference color_reference = vk::AttachmentReference()
      .attachment(0)
      .layout(vk::ImageLayout::eColorAttachmentOptimal);
  const vk::AttachmentReference depth_reference = vk::AttachmentReference()
      .attachment(1)
      .layout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
  const vk::SubpassDescription subpass = vk::SubpassDescription()
      .pipelineBindPoint(vk::PipelineBindPoint::eGraphics)
      .colorAttachmentCount(1)
      .pColorAttachments(&color_reference)
      .pDepthStencilAttachment(&depth_reference);
  const vk::RenderPassCreateInfo rp_info = vk::RenderPassCreateInfo()
      .attachmentCount(2)
      .pAttachments(attachments)
      .subpassCount(1)
      .pSubpasses(&subpass);

  vk::chk(vkDevice().createRenderPass(&rp_info, nullptr, &render_pass_));
}

void DemoScene::PrepareDescriptorPool() {
  const vk::DescriptorPoolSize type_count[] = {
    vk::DescriptorPoolSize()
      .type(vk::DescriptorType::eCombinedImageSampler)
      .descriptorCount(DEMO_TEXTURE_COUNT),
    vk::DescriptorPoolSize()
      .type(vk::DescriptorType::eUniformBuffer)
      .descriptorCount(1)
  };

  const vk::DescriptorPoolCreateInfo descriptor_pool =
    vk::DescriptorPoolCreateInfo()
      .maxSets(1)
      .poolSizeCount(2)
      .pPoolSizes(type_count);

  vk::chk(vkDevice().createDescriptorPool(&descriptor_pool, nullptr,
                                          &desc_pool_));
}

void DemoScene::PrepareUniformBuffer() {
  vk::BufferCreateInfo buf_info = vk::BufferCreateInfo{}
      .usage(vk::BufferUsageFlagBits::eUniformBuffer)
      .size(sizeof(UniformData))
      .sharingMode(vk::SharingMode::eExclusive);
  vk::chk(vkDevice().createBuffer(&buf_info, NULL, &uniform_data_.buf));

  vk::MemoryRequirements mem_reqs;
  vkDevice().getBufferMemoryRequirements(uniform_data_.buf, &mem_reqs);

  vk::MemoryAllocateInfo alloc_info;
  alloc_info.allocationSize(mem_reqs.size());
  MemoryTypeFromProperties(vkGpuMemoryProperties(),
                           mem_reqs.memoryTypeBits(),
                           vk::MemoryPropertyFlagBits::eHostVisible,
                           alloc_info);

  vk::chk(vkDevice().allocateMemory(&alloc_info, NULL,
                                          &uniform_data_.mem));

  vk::chk(vkDevice().bindBufferMemory(uniform_data_.buf,
                                            uniform_data_.mem, 0));

  uniform_data_.bufferInfo.buffer(uniform_data_.buf);
  uniform_data_.bufferInfo.offset(0);
  uniform_data_.bufferInfo.range(sizeof(UniformData));
}

void DemoScene::PrepareDescriptorSet() {
  vk::DescriptorImageInfo tex_descs[DEMO_TEXTURE_COUNT];
  vk::WriteDescriptorSet writes[2];

  vk::DescriptorSetAllocateInfo alloc_info =
    vk::DescriptorSetAllocateInfo()
      .descriptorPool(desc_pool_)
      .descriptorSetCount(1)
      .pSetLayouts(&desc_layout_);
  vk::chk(vkDevice().allocateDescriptorSets(&alloc_info, &desc_set_));

  for (uint32_t i = 0; i < DEMO_TEXTURE_COUNT; i++) {
      tex_descs[i].sampler(textures_[i].sampler);
      tex_descs[i].imageView(textures_[i].view);
      tex_descs[i].imageLayout(vk::ImageLayout::eGeneral);
  }

  writes[0].dstBinding(0);
  writes[0].dstSet(desc_set_);
  writes[0].descriptorCount(DEMO_TEXTURE_COUNT);
  writes[0].descriptorType(vk::DescriptorType::eCombinedImageSampler);
  writes[0].pImageInfo(tex_descs);

  writes[1].dstBinding(1);
  writes[1].dstSet(desc_set_);
  writes[1].descriptorCount(1);
  writes[1].descriptorType(vk::DescriptorType::eUniformBuffer);
  writes[1].pBufferInfo(&uniform_data_.bufferInfo);

  vkDevice().updateDescriptorSets(2, writes, 0, nullptr);
}

void DemoScene::PrepareFramebuffers() {
    vk::ImageView attachments[2];
    attachments[1] = vkDepthBuffer().view;

    const vk::FramebufferCreateInfo fb_info =
      vk::FramebufferCreateInfo()
        .renderPass(render_pass_)
        .attachmentCount(2)
        .pAttachments(attachments)
        .width(framebufferSize().x)
        .height(framebufferSize().y)
        .layers(1);

    framebuffers_ = std::unique_ptr<vk::Framebuffer>{
        new vk::Framebuffer[vkSwapchainImageCount()]};

    for (uint32_t i = 0; i < vkSwapchainImageCount(); i++) {
        attachments[0] = vkBuffers()[i].view;
        vk::chk(vkDevice().createFramebuffer(&fb_info, nullptr,
                                             &framebuffers_.get()[i]));
    }
}

void DemoScene::Prepare() {
    PrepareTextures();
    PrepareVertices();
    PrepareIndices();

    PrepareDescriptorLayout();
    PrepareUniformBuffer();
    PrepareDescriptorPool();
    PrepareDescriptorSet();

    PrepareRenderPass();
    pipeline_ = Initialize::PreparePipeline(vkDevice(), vertex_input_,
                                            pipeline_layout_, render_pass_);

    PrepareFramebuffers();
}

void DemoScene::Cleanup() {
    for (uint32_t i = 0; i < vkSwapchainImageCount(); i++) {
        vkDevice().destroyFramebuffer(framebuffers_.get()[i], nullptr);
    }
    vkDevice().destroyDescriptorPool(desc_pool_, nullptr);

    vkDevice().destroyPipeline(pipeline_, nullptr);
    vkDevice().destroyRenderPass(render_pass_, nullptr);
    vkDevice().destroyPipelineLayout(pipeline_layout_, nullptr);
    vkDevice().destroyDescriptorSetLayout(desc_layout_, nullptr);

    vkDevice().destroyBuffer(uniform_data_.buf, nullptr);
    vkDevice().freeMemory(uniform_data_.mem, nullptr);

    vkDevice().destroyBuffer(vertex_attribs_.buf, nullptr);
    vkDevice().freeMemory(vertex_attribs_.mem, nullptr);
    vkDevice().destroyBuffer(instance_attribs_.buf, nullptr);
    vkDevice().freeMemory(instance_attribs_.mem, nullptr);
    vkDevice().destroyBuffer(indices_.buf, nullptr);
    vkDevice().freeMemory(indices_.mem, nullptr);

    for (uint32_t i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        vkDevice().destroyImageView(textures_[i].view, nullptr);
        vkDevice().destroyImage(textures_[i].image, nullptr);
        vkDevice().freeMemory(textures_[i].mem, nullptr);
        vkDevice().destroySampler(textures_[i].sampler, nullptr);
    }
}

#include <ctime>
static clock_t startTime;
static int renderedFrames;

DemoScene::DemoScene(GLFWwindow *window)
    : Scene(window)
    , quad_trees_{
        {Settings::kFaceSize, CubeFace::kPosX},
        {Settings::kFaceSize, CubeFace::kNegX},
        {Settings::kFaceSize, CubeFace::kPosY},
        {Settings::kFaceSize, CubeFace::kNegY},
        {Settings::kFaceSize, CubeFace::kPosZ},
        {Settings::kFaceSize, CubeFace::kNegZ},
      } {
  Prepare();
  setCamera(AddComponent<engine::FreeFlyCamera>(glm::radians(60.0), 1, 100000, glm::dvec3(0, 10, 0), glm::dvec3{10, 0, 10}, 1000));
  startTime = clock();
}

DemoScene::~DemoScene() {
  Cleanup();
  clock_t endTime = clock();
  double elapsedSecs = double(endTime - startTime) / CLOCKS_PER_SEC;
  std::cout << "Average FPS: " << renderedFrames / elapsedSecs << std::endl;
}

void DemoScene::Render() {
  Draw();
  renderedFrames++;
}

void DemoScene::Update() {
  {
    glm::mat4 mvp = scene()->camera()->projectionMatrix() *
                    scene()->camera()->cameraMatrix();

    UniformData* uniform_data;
    vk::chk(vkDevice().mapMemory(uniform_data_.mem, 0, sizeof(mvp),
                                 vk::MemoryMapFlags{}, (void **)&uniform_data));

    uniform_data->mvp = mvp;
    uniform_data->camera_pos = scene()->camera()->transform().pos();
    uniform_data->terrain_smallest_geometry_lod_distance = Settings::kSmallestGeometryLodDistance;
    uniform_data->terrain_sphere_radius = Settings::kSphereRadius;
    uniform_data->terrain_max_lod_level = quad_trees_[0].max_node_level();

    vkDevice().unmapMemory(uniform_data_.mem);
  }

  // update instances to draw
  grid_mesh_.ClearRenderList();
  for (CdlodQuadTree& quad_tree : quad_trees_) {
    quad_tree.SelectNodes(*scene()->camera(), grid_mesh_);
  }

  if (grid_mesh_.mesh_.render_data_.size() > Settings::kMaxInstanceCount) {
    std::cerr << "Number of instances used: " << grid_mesh_.mesh_.render_data_.size() << std::endl;
    std::terminate();
  }

  {
    glm::vec4 *render_data_;
    vk::chk(vkDevice().mapMemory(instance_attribs_.mem, 0,
                                 Settings::kMaxInstanceCount * sizeof(glm::vec4),
                                 vk::MemoryMapFlags{}, (void **)&render_data_));

    for (int i = 0; i < grid_mesh_.mesh_.render_data_.size(); ++i) {
      render_data_[i] = grid_mesh_.mesh_.render_data_[i];
    }

    vkDevice().unmapMemory(instance_attribs_.mem);
  }
}


void DemoScene::ScreenResizedClean() {
  Cleanup();
  Scene::ScreenResizedClean();
}

void DemoScene::ScreenResized(size_t width, size_t height) {
  Scene::ScreenResized(width, height);
  Prepare();
}
