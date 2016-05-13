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
#include <cstddef>

#include <vulkan/vk_cpp.hpp>
#include <GLFW/glfw3.h>

#include "engine/scene.hpp"
#include "common/error_checking.hpp"
#include "common/vulkan_memory.hpp"
#include "common/file_utils.hpp"
#include "shader/glsl2spv.hpp"

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
      .framebuffer(framebuffers_.get()[vk_current_buffer()])
      .renderArea({vk::Offset2D(0, 0),
          vk::Extent2D(framebuffer_size().x, framebuffer_size().y)})
      .clearValueCount(2)
      .pClearValues(clear_values);

  vk::chk(vk_draw_cmd().begin(&cmd_buf_info));

  vk_draw_cmd().beginRenderPass(&rp_begin, vk::SubpassContents::eInline);
  vk_draw_cmd().bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_);
  vk_draw_cmd().bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
      pipeline_layout_, 0, 1, &desc_set_, 0, nullptr);

  vk::Viewport viewport = vk::Viewport()
    .width(framebuffer_size().x)
    .height(framebuffer_size().y)
    .minDepth(0.0f)
    .maxDepth(1.0f);
  vk_draw_cmd().setViewport(0, 1, &viewport);

  vk::Rect2D scissor{vk::Offset2D(0, 0), vk::Extent2D(framebuffer_size().x, framebuffer_size().y)};
  vk_draw_cmd().setScissor(0, 1, &scissor);

  vk::DeviceSize offsets[1] = {0};
  vk_draw_cmd().bindVertexBuffers(VERTEX_BUFFER_BIND_ID, 1,
                                  &vertex_attribs_.buf, offsets);
  vk_draw_cmd().bindVertexBuffers(INSTANCE_BUFFER_BIND_ID, 1,
                                  &instance_attribs_.buf, offsets);
  vk_draw_cmd().bindIndexBuffer(indices_.buf,
                                vk::DeviceSize{},
                                vk::IndexType::eUint16);

  vk_draw_cmd().drawIndexed(grid_mesh_.mesh_.index_count_,
                            grid_mesh_.mesh_.attribs_.size(), 0, 0, 0);
  vk_draw_cmd().endRenderPass();

  vk::ImageMemoryBarrier pre_present_barrier = vk::ImageMemoryBarrier()
      .srcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
      .dstAccessMask(vk::AccessFlagBits::eMemoryRead)
      // .oldLayout(vk::ImageLayout::eColorAttachmentOptimal)
      .oldLayout(vk::ImageLayout::ePresentSrcKHR)
      .newLayout(vk::ImageLayout::ePresentSrcKHR)
      .srcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
      .dstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
      .subresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

  pre_present_barrier.image(vk_buffers()[vk_current_buffer()].image);
  vk_draw_cmd().pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                                vk::PipelineStageFlagBits::eBottomOfPipe,
                                vk::DependencyFlags(), 0, nullptr, 0,
                                nullptr, 1, &pre_present_barrier);

  vk_draw_cmd().end();
}

void DemoScene::Draw() {
  vk::Semaphore present_complete_semaphore;
  vk::SemaphoreCreateInfo present_complete_semaphore_create_info;

  vk::chk(vk_device().createSemaphore(&present_complete_semaphore_create_info,
                                     nullptr, &present_complete_semaphore));

  // Get the index of the next available swapchain image:
  VkResult vkErr = vk_app().entry_points.AcquireNextImageKHR(
      vk_device(), vk_swapchain(), UINT64_MAX, present_complete_semaphore,
      (vk::Fence)nullptr, &vk_current_buffer());
  if (vkErr == VK_ERROR_OUT_OF_DATE_KHR) {
      // vk_swapchain() is out of date (e.g. the window was resized) and
      // must be recreated:
      //demo_resize(demo, scene);
      throw std::runtime_error("Should resize swapchain");
      Draw();
      vk_device().destroySemaphore(present_complete_semaphore, nullptr);
      return;
  } else if (vkErr == VK_SUBOPTIMAL_KHR) {
      // vk_swapchain() is not as optimal as it could be, but the platform's
      // presentation engine will still present the image correctly.
  } else {
      assert(vkErr == VK_SUCCESS);
  }

  // Assume the command buffer has been run on current_buffer before so
  // we need to set the image layout back to COLOR_ATTACHMENT_OPTIMAL
  // SetImageLayout(vk_buffers()[vk_current_buffer()].image,
  //                vk::ImageAspectFlagBits::eColor,
  //                vk::ImageLayout::ePresentSrcKHR,
  //                vk::ImageLayout::eColorAttachmentOptimal,
  //                vk::AccessFlags{});
  // FlushInitCommand();

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
      .pCommandBuffers(&vk_draw_cmd());

  vk::chk(vk_queue().submit(1, &submit_info, null_fence));

  vk::PresentInfoKHR present = vk::PresentInfoKHR()
      .swapchainCount(1)
      .pSwapchains(&vk_swapchain())
      .pImageIndices(&vk_current_buffer());
  VkPresentInfoKHR& vkPresent =
    const_cast<VkPresentInfoKHR&>(
      static_cast<const VkPresentInfoKHR&>(present));

  // TBD/TODO: SHOULD THE "present" PARAMETER BE "const" IN THE HEADER?
  vkErr = vk_app().entry_points.QueuePresentKHR(vk_queue(), &vkPresent);
  if (vkErr == VK_ERROR_OUT_OF_DATE_KHR) {
      // vk_swapchain() is out of date (e.g. the window was resized) and
      // must be recreated:
      //demo_resize(demo, scene);
      throw std::runtime_error("Should resize swapchain");
  } else if (vkErr == VK_SUBOPTIMAL_KHR) {
      // vk_swapchain() is not as optimal as it could be, but the platform's
      // presentation engine will still present the image correctly.
  } else {
      assert(vkErr == VK_SUCCESS);
  }

  vk_queue().waitIdle();

  vk_device().destroySemaphore(present_complete_semaphore, nullptr);
}

void DemoScene::PrepareTextureImage(const unsigned char *tex_colors,
                                    int tex_width, int tex_height,
                                    TextureObject *tex_obj, vk::ImageTiling tiling,
                                    vk::ImageUsageFlags usage,
                                    vk::MemoryPropertyFlags required_props,
                                    vk::Format tex_format,
                                    size_t byte_per_texel) {
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

  vk::chk(vk_device().createImage(&image_create_info, nullptr, &tex_obj->image));

  vk_device().getImageMemoryRequirements(tex_obj->image, &mem_reqs);

  mem_alloc.allocationSize(mem_reqs.size());
  MemoryTypeFromProperties(vk_gpu_memory_properties(),
                           mem_reqs.memoryTypeBits(),
                           required_props, mem_alloc);

  vk::chk(vk_device().allocateMemory(&mem_alloc, nullptr, &tex_obj->mem));

  vk_device().bindImageMemory(tex_obj->image, tex_obj->mem, 0);

  if (required_props & vk::MemoryPropertyFlagBits::eHostVisible) {
      const vk::ImageSubresource subres =
        vk::ImageSubresource().aspectMask(vk::ImageAspectFlagBits::eColor);
      vk::SubresourceLayout layout;

      vk_device().getImageSubresourceLayout(tex_obj->image, &subres, &layout);

      void *data = vk_device().mapMemory(tex_obj->mem, 0,
                                         mem_alloc.allocationSize(),
                                         vk::MemoryMapFlags{});

      for (int y = 0; y < tex_height; y++) {
          char *row = ((char *)data + layout.rowPitch() * y);
          for (int x = 0; x < tex_width; x++) {
            std::memcpy(&row[x * byte_per_texel],
                        &tex_colors[(y*tex_width + x) * byte_per_texel],
                        byte_per_texel);
          }
      }

      vk_device().unmapMemory(tex_obj->mem);
  }

  tex_obj->imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  SetImageLayout(tex_obj->image, vk::ImageAspectFlagBits::eColor,
                 vk::ImageLayout::eUndefined, tex_obj->imageLayout,
                 vk::AccessFlags{});
  /* setting the image layout does not reference the actual memory so no need
   * to add a mem ref */
}

void DemoScene::SetupTexture(size_t index, unsigned width, unsigned height,
                             vk::Format tex_format, size_t byte_per_texel,
                             const unsigned char* pixels) {
  int succesfully_removed = unused_indices_.erase(index);
  bool first_texture = used_index_count_ == 0;

  assert(succesfully_removed);
  if (used_index_count_ <= index) {
    used_index_count_ = index + 1;
  } else { // this was already used before, time to free it
    vk_device().destroyImageView(textures_[index].view, nullptr);
    vk_device().destroyImage(textures_[index].image, nullptr);
    vk_device().freeMemory(textures_[index].mem, nullptr);
    vk_device().destroySampler(textures_[index].sampler, nullptr);
  }

  vk::FormatProperties props;
  vk_gpu().getFormatProperties(tex_format, &props);

  if (props.optimalTilingFeatures() & vk::FormatFeatureFlagBits::eSampledImage) {
    /* Must use staging buffer to copy linear texture to optimized */
    TextureObject staging_texture;

    PrepareTextureImage(pixels, width, height, &staging_texture,
                        vk::ImageTiling::eLinear,
                        vk::ImageUsageFlagBits::eTransferSrc,
                        vk::MemoryPropertyFlagBits::eHostVisible,
                        tex_format, byte_per_texel);

    PrepareTextureImage(pixels,
        width, height, &textures_[index],
        vk::ImageTiling::eOptimal,
        (vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled),
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        tex_format, byte_per_texel);

    SetImageLayout(staging_texture.image,
                   vk::ImageAspectFlagBits::eColor,
                   staging_texture.imageLayout,
                   vk::ImageLayout::eTransferSrcOptimal,
                   vk::AccessFlagBits::eShaderRead);

    SetImageLayout(textures_[index].image,
                   vk::ImageAspectFlagBits::eColor,
                   textures_[index].imageLayout,
                   vk::ImageLayout::eTransferDstOptimal,
                   vk::AccessFlagBits::eShaderRead);

    vk::ImageCopy copy_region = vk::ImageCopy()
        .srcSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
        .dstSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
        .extent(vk::Extent3D(staging_texture.tex_width,
                             staging_texture.tex_height, 1));
    vk_setup_cmd().copyImage(staging_texture.image,
        vk::ImageLayout::eTransferSrcOptimal, textures_[index].image,
        vk::ImageLayout::eTransferDstOptimal, 1, &copy_region);

    SetImageLayout(textures_[index].image,
                   vk::ImageAspectFlagBits::eColor,
                   vk::ImageLayout::eTransferDstOptimal,
                   textures_[index].imageLayout,
                   vk::AccessFlagBits::eTransferWrite);

    // FlushInitCommand();

    //vk_device().destroyImage(staging_texture.image, nullptr);
    //vk_device().freeMemory(staging_texture.mem, nullptr);
  } else {
    /* Can't support vk::Format::eR16G16B16A16Unorm !? */
    assert(!"No support for eR16G16B16A16Unorm as texture image format");
  }

  const vk::SamplerCreateInfo sampler = vk::SamplerCreateInfo()
      .magFilter(vk::Filter::eLinear)
      .minFilter(vk::Filter::eLinear)
      .mipmapMode(vk::SamplerMipmapMode::eNearest)
      .addressModeU(vk::SamplerAddressMode::eClampToEdge)
      .addressModeV(vk::SamplerAddressMode::eClampToEdge)
      .addressModeW(vk::SamplerAddressMode::eClampToEdge)
      .mipLodBias(0.0f)
      .anisotropyEnable(VK_TRUE)
      .maxAnisotropy(16)
      .compareOp(vk::CompareOp::eNever)
      .minLod(0.0f)
      .maxLod(0.0f)
      .borderColor(vk::BorderColor::eFloatOpaqueWhite)
      .unnormalizedCoordinates(VK_FALSE);

  vk::ImageViewCreateInfo view = vk::ImageViewCreateInfo()
      .viewType(vk::ImageViewType::e2D)
      .format(tex_format)
      .subresourceRange(vk::ImageSubresourceRange{}
          .aspectMask(vk::ImageAspectFlagBits::eColor)
          .baseMipLevel(0)
          .levelCount(1)
          .baseArrayLayer(0)
          .layerCount(1));

  /* create sampler */
  vk::chk(vk_device().createSampler(&sampler, nullptr,
                                    &textures_[index].sampler));

  /* create image view */
  view.image(textures_[index].image);
  vk::chk(vk_device().createImageView(&view, nullptr, &textures_[index].view));

  /* Update the descriptor set info */
  tex_descriptor_infos_[index].sampler(textures_[index].sampler);
  tex_descriptor_infos_[index].imageView(textures_[index].view);
  tex_descriptor_infos_[index].imageLayout(vk::ImageLayout::eGeneral);

  if (first_texture) {
    // fill all of the descriptors with the first (root) texture
    // just to avoid warning for uninitialized descriptor slots, that are being
    // used while rendering
    size_t descriptor_count = Settings::kMaxTextureCount;
    for (int i = 0; i < descriptor_count; ++i) {
      tex_descriptor_infos_[i].sampler(textures_[index].sampler);
      tex_descriptor_infos_[i].imageView(textures_[index].view);
      tex_descriptor_infos_[i].imageLayout(vk::ImageLayout::eGeneral);
    }

    vk::WriteDescriptorSet writes[1];

    writes[0].dstBinding(0);
    writes[0].dstSet(desc_set_);
    writes[0].descriptorCount(descriptor_count);
    writes[0].descriptorType(vk::DescriptorType::eCombinedImageSampler);
    writes[0].pImageInfo(tex_descriptor_infos_);

    vk_device().updateDescriptorSets(1, writes, 0, nullptr);
  }
}

void DemoScene::FreeTexture(size_t index) {
  unused_indices_.insert(index);
}

void DemoScene::PrepareIndices() {
  const std::vector<uint16_t>& indices = grid_mesh_.mesh_.indices_;
  const vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
      .size(sizeof(uint16_t) * indices.size())
      .usage(vk::BufferUsageFlagBits::eIndexBuffer);

  vk::MemoryAllocateInfo mem_alloc;
  vk::MemoryRequirements mem_reqs;

  vk::chk(vk_device().createBuffer(&buf_info, nullptr, &indices_.buf));

  vk_device().getBufferMemoryRequirements(indices_.buf, &mem_reqs);

  mem_alloc.allocationSize(mem_reqs.size());
  MemoryTypeFromProperties(vk_gpu_memory_properties(),
                           mem_reqs.memoryTypeBits(),
                           vk::MemoryPropertyFlagBits::eHostVisible,
                           mem_alloc);

  vk::chk(vk_device().allocateMemory(&mem_alloc, nullptr, &indices_.mem));

  void *data = vk_device().mapMemory(indices_.mem, 0, mem_alloc.allocationSize(),
                                     vk::MemoryMapFlags{});

  std::memcpy(data, indices.data(), sizeof(uint16_t) * indices.size());

  vk_device().unmapMemory(indices_.mem);

  vk_device().bindBufferMemory(indices_.buf, indices_.mem, 0);
}

void DemoScene::PrepareVertices() {
  vk::MemoryAllocateInfo mem_alloc;
  vk::MemoryRequirements mem_reqs;

  { // vertexAttribs
    const vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
      .size(sizeof(svec2) * grid_mesh_.mesh_.positions_.size())
      .usage(vk::BufferUsageFlagBits::eVertexBuffer);

    vk::chk(vk_device().createBuffer(&buf_info, nullptr, &vertex_attribs_.buf));

    vk_device().getBufferMemoryRequirements(vertex_attribs_.buf, &mem_reqs);

    mem_alloc.allocationSize(mem_reqs.size());
    MemoryTypeFromProperties(vk_gpu_memory_properties(),
                             mem_reqs.memoryTypeBits(),
                             vk::MemoryPropertyFlagBits::eHostVisible,
                             mem_alloc);

    vk::chk(vk_device().allocateMemory(&mem_alloc, nullptr, &vertex_attribs_.mem));
    vk_device().bindBufferMemory(vertex_attribs_.buf, vertex_attribs_.mem, 0);

    void *data = vk_device().mapMemory(vertex_attribs_.mem, 0,
                                       mem_alloc.allocationSize(),
                                       vk::MemoryMapFlags{});

    std::memcpy(data, grid_mesh_.mesh_.positions_.data(),
                sizeof(svec2) * grid_mesh_.mesh_.positions_.size());

    vk_device().unmapMemory(vertex_attribs_.mem);
  }

  { // instanceAttribs
    const vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
      .size(sizeof(PerInstanceAttributes) * Settings::kMaxInstanceCount)
      .usage(vk::BufferUsageFlagBits::eVertexBuffer);

    vk::chk(vk_device().createBuffer(&buf_info, nullptr, &instance_attribs_.buf));

    vk_device().getBufferMemoryRequirements(instance_attribs_.buf, &mem_reqs);

    mem_alloc.allocationSize(mem_reqs.size());
    MemoryTypeFromProperties(vk_gpu_memory_properties(),
                             mem_reqs.memoryTypeBits(),
                             vk::MemoryPropertyFlagBits::eHostVisible,
                             mem_alloc);

    vk::chk(vk_device().allocateMemory(&mem_alloc, nullptr, &instance_attribs_.mem));
    vk_device().bindBufferMemory(instance_attribs_.buf, instance_attribs_.mem, 0);
  }

  vertex_input_bindings_[0].binding(VERTEX_BUFFER_BIND_ID);
  vertex_input_bindings_[0].stride(sizeof(svec2));
  vertex_input_bindings_[0].inputRate(vk::VertexInputRate::eVertex);

  vertex_input_bindings_[1].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_bindings_[1].stride(sizeof(PerInstanceAttributes));
  vertex_input_bindings_[1].inputRate(vk::VertexInputRate::eInstance);

  vertex_input_attribs_[0].binding(VERTEX_BUFFER_BIND_ID);
  vertex_input_attribs_[0].location(0);
  vertex_input_attribs_[0].format(vk::Format::eR16G16Sint);
  vertex_input_attribs_[0].offset(0);

  vertex_input_attribs_[1].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[1].location(1);
  vertex_input_attribs_[1].format(vk::Format::eR32G32B32A32Sfloat);
  vertex_input_attribs_[1].offset(offsetof(PerInstanceAttributes, render_data));

  vertex_input_attribs_[2].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[2].location(2);
  vertex_input_attribs_[2].format(vk::Format::eR32Uint);
  vertex_input_attribs_[2].offset(offsetof(PerInstanceAttributes,
                                           texture_info.geometry_current.id));

  vertex_input_attribs_[3].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[3].location(3);
  vertex_input_attribs_[3].format(vk::Format::eR32G32B32Sfloat);
  vertex_input_attribs_[3].offset(offsetof(PerInstanceAttributes,
                                           texture_info.geometry_current.position));

  vertex_input_attribs_[4].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[4].location(4);
  vertex_input_attribs_[4].format(vk::Format::eR32Uint);
  vertex_input_attribs_[4].offset(offsetof(PerInstanceAttributes,
                                           texture_info.geometry_next.id));

  vertex_input_attribs_[5].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[5].location(5);
  vertex_input_attribs_[5].format(vk::Format::eR32G32B32Sfloat);
  vertex_input_attribs_[5].offset(offsetof(PerInstanceAttributes,
                                           texture_info.geometry_next.position));

  vertex_input_attribs_[6].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[6].location(6);
  vertex_input_attribs_[6].format(vk::Format::eR32Uint);
  vertex_input_attribs_[6].offset(offsetof(PerInstanceAttributes,
                                           texture_info.normal_current.id));

  vertex_input_attribs_[7].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[7].location(7);
  vertex_input_attribs_[7].format(vk::Format::eR32G32B32Sfloat);
  vertex_input_attribs_[7].offset(offsetof(PerInstanceAttributes,
                                           texture_info.normal_current.position));

  vertex_input_attribs_[8].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[8].location(8);
  vertex_input_attribs_[8].format(vk::Format::eR32Uint);
  vertex_input_attribs_[8].offset(offsetof(PerInstanceAttributes,
                                           texture_info.normal_next.id));

  vertex_input_attribs_[9].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[9].location(9);
  vertex_input_attribs_[9].format(vk::Format::eR32G32B32Sfloat);
  vertex_input_attribs_[9].offset(offsetof(PerInstanceAttributes,
                                           texture_info.normal_next.position));

  vertex_input_attribs_[10].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[10].location(10);
  vertex_input_attribs_[10].format(vk::Format::eR32Uint);
  vertex_input_attribs_[10].offset(offsetof(PerInstanceAttributes,
                                            texture_info.diffuse_current.id));

  vertex_input_attribs_[11].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[11].location(11);
  vertex_input_attribs_[11].format(vk::Format::eR32G32B32Sfloat);
  vertex_input_attribs_[11].offset(offsetof(PerInstanceAttributes,
                                            texture_info.diffuse_current.position));

  vertex_input_attribs_[12].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[12].location(12);
  vertex_input_attribs_[12].format(vk::Format::eR32Uint);
  vertex_input_attribs_[12].offset(offsetof(PerInstanceAttributes,
                                            texture_info.diffuse_next.id));

  vertex_input_attribs_[13].binding(INSTANCE_BUFFER_BIND_ID);
  vertex_input_attribs_[13].location(13);
  vertex_input_attribs_[13].format(vk::Format::eR32G32B32Sfloat);
  vertex_input_attribs_[13].offset(offsetof(PerInstanceAttributes,
                                            texture_info.diffuse_next.position));

  vertex_input_.vertexBindingDescriptionCount(2);
  vertex_input_.pVertexBindingDescriptions(vertex_input_bindings_);
  vertex_input_.vertexAttributeDescriptionCount(14);
  vertex_input_.pVertexAttributeDescriptions(vertex_input_attribs_);
}

void DemoScene::PrepareDescriptorLayout() {
  const vk::DescriptorSetLayoutBinding layout_binding[] = {
    vk::DescriptorSetLayoutBinding()
      .binding(0)
      .descriptorType(vk::DescriptorType::eCombinedImageSampler)
      .descriptorCount(Settings::kMaxTextureCount)
      .stageFlags(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex),
    vk::DescriptorSetLayoutBinding()
      .binding(1)
      .descriptorType(vk::DescriptorType::eUniformBuffer)
      .descriptorCount(1)
      .stageFlags(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex)
  };

  const vk::DescriptorSetLayoutCreateInfo descriptor_layout =
    vk::DescriptorSetLayoutCreateInfo()
      .bindingCount(2)
      .pBindings(layout_binding);

  vk::chk(vk_device().createDescriptorSetLayout(&descriptor_layout, nullptr,
                                                &desc_layout_));

  const vk::PipelineLayoutCreateInfo pipeline_layout_create_info =
    vk::PipelineLayoutCreateInfo()
      .setLayoutCount(1)
      .pSetLayouts(&desc_layout_);

  vk::chk(vk_device().createPipelineLayout(&pipeline_layout_create_info,
                                          nullptr, &pipeline_layout_));
}

void DemoScene::PrepareRenderPass() {
  const vk::AttachmentDescription attachments[2] = {
    vk::AttachmentDescription()
      .format(vk_surface_format())
      .samples(vk::SampleCountFlagBits::e1)
      .loadOp(vk::AttachmentLoadOp::eClear)
      .storeOp(vk::AttachmentStoreOp::eStore)
      .stencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .stencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      // .initialLayout(vk::ImageLayout::eColorAttachmentOptimal)
      .initialLayout(vk::ImageLayout::ePresentSrcKHR)
      // .finalLayout(vk::ImageLayout::eColorAttachmentOptimal),
      .finalLayout(vk::ImageLayout::ePresentSrcKHR),
    vk::AttachmentDescription()
      .format(vk_depth_buffer().format)
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
      // .layout(vk::ImageLayout::eColorAttachmentOptimal);
      .layout(vk::ImageLayout::ePresentSrcKHR);
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

  vk::chk(vk_device().createRenderPass(&rp_info, nullptr, &render_pass_));
}

void DemoScene::PrepareDescriptorPool() {
  const vk::DescriptorPoolSize type_count[] = {
    vk::DescriptorPoolSize()
      .type(vk::DescriptorType::eCombinedImageSampler)
      .descriptorCount(Settings::kMaxTextureCount),
    vk::DescriptorPoolSize()
      .type(vk::DescriptorType::eUniformBuffer)
      .descriptorCount(1)
  };

  const vk::DescriptorPoolCreateInfo descriptor_pool =
    vk::DescriptorPoolCreateInfo()
      .maxSets(1)
      .poolSizeCount(2)
      .pPoolSizes(type_count);

  vk::chk(vk_device().createDescriptorPool(&descriptor_pool, nullptr,
                                           &desc_pool_));
}

void DemoScene::PrepareUniformBuffer() {
  vk::BufferCreateInfo buf_info = vk::BufferCreateInfo{}
      .usage(vk::BufferUsageFlagBits::eUniformBuffer)
      .size(sizeof(UniformData))
      .sharingMode(vk::SharingMode::eExclusive);
  vk::chk(vk_device().createBuffer(&buf_info, NULL, &uniform_data_.buf));

  vk::MemoryRequirements mem_reqs;
  vk_device().getBufferMemoryRequirements(uniform_data_.buf, &mem_reqs);

  vk::MemoryAllocateInfo alloc_info;
  alloc_info.allocationSize(mem_reqs.size());
  MemoryTypeFromProperties(vk_gpu_memory_properties(),
                           mem_reqs.memoryTypeBits(),
                           vk::MemoryPropertyFlagBits::eHostVisible,
                           alloc_info);

  vk::chk(vk_device().allocateMemory(&alloc_info, NULL,
                                     &uniform_data_.mem));

  vk_device().bindBufferMemory(uniform_data_.buf, uniform_data_.mem, 0);

  uniform_data_.buffer_info.buffer(uniform_data_.buf);
  uniform_data_.buffer_info.offset(0);
  uniform_data_.buffer_info.range(sizeof(UniformData));
}

void DemoScene::PrepareDescriptorSet() {
  vk::DescriptorSetAllocateInfo alloc_info =
    vk::DescriptorSetAllocateInfo()
      .descriptorPool(desc_pool_)
      .descriptorSetCount(1)
      .pSetLayouts(&desc_layout_);
  vk::chk(vk_device().allocateDescriptorSets(&alloc_info, &desc_set_));


  vk::WriteDescriptorSet writes[1];

  writes[0].dstBinding(1);
  writes[0].dstSet(desc_set_);
  writes[0].descriptorCount(1);
  writes[0].descriptorType(vk::DescriptorType::eUniformBuffer);
  writes[0].pBufferInfo(&uniform_data_.buffer_info);

  vk_device().updateDescriptorSets(1, writes, 0, nullptr);
}

static vk::ShaderModule PrepareShaderModule(const vk::Device& device,
                                            const void *code,
                                            size_t size) {
  vk::ShaderModuleCreateInfo module_create_info;
  vk::ShaderModule module;

  module_create_info.codeSize(size);
  module_create_info.pCode(static_cast<const uint32_t*>(code));
  vk::chk(device.createShaderModule(&module_create_info, nullptr, &module));

  return module;
}

static vk::ShaderModule PrepareVs(const vk::Device& device) {
  std::vector<unsigned int> vert_shader =
    Shader::GLSLtoSPV(vk::ShaderStageFlagBits::eVertex,
                      FileUtils::ReadFileToString("src/glsl/simple.vert"));

  return PrepareShaderModule(device, (const void*)vert_shader.data(),
                             vert_shader.size() * sizeof(vert_shader[0]));
}

static vk::ShaderModule PrepareFs(const vk::Device& device) {
  std::vector<unsigned int> frag_shader =
    Shader::GLSLtoSPV(vk::ShaderStageFlagBits::eFragment,
                      FileUtils::ReadFileToString("src/glsl/simple.frag"));

  return PrepareShaderModule(device, (const void*)frag_shader.data(),
                             frag_shader.size() * sizeof(frag_shader[0]));
}

static vk::Pipeline PreparePipeline(
          const vk::Device& device,
          const vk::PipelineVertexInputStateCreateInfo& vertexState,
          const vk::PipelineLayout& pipelineLayout,
          const vk::RenderPass& renderPass) {
  vk::GraphicsPipelineCreateInfo pipeline_create_info;

  vk::PipelineInputAssemblyStateCreateInfo ia;
  vk::PipelineRasterizationStateCreateInfo rs;
  vk::PipelineColorBlendStateCreateInfo cb;
  vk::PipelineDepthStencilStateCreateInfo ds;
  vk::PipelineViewportStateCreateInfo vp;
  vk::PipelineMultisampleStateCreateInfo ms;
  vk::DynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE];
  vk::PipelineDynamicStateCreateInfo dynamic_state;

  dynamic_state.pDynamicStates(dynamicStateEnables);
  pipeline_create_info.layout(pipelineLayout);
  ia.topology(vk::PrimitiveTopology::eTriangleList);

  if (Settings::kWireframe) {
    rs.lineWidth(2.0);
    rs.polygonMode(vk::PolygonMode::eLine);
    rs.cullMode(vk::CullModeFlagBits::eNone);
  } else {
    rs.lineWidth(1.0);
    rs.polygonMode(vk::PolygonMode::eFill);
    rs.cullMode(vk::CullModeFlagBits::eBack);
  }
  rs.frontFace(vk::FrontFace::eCounterClockwise);
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
  dynamicStateEnables[dynamic_state.dynamicStateCount()] =
      vk::DynamicState::eViewport;
  dynamic_state.dynamicStateCount(dynamic_state.dynamicStateCount() + 1);

  vp.scissorCount(1);
  dynamicStateEnables[dynamic_state.dynamicStateCount()] =
      vk::DynamicState::eScissor;
  dynamic_state.dynamicStateCount(dynamic_state.dynamicStateCount() + 1);

  ds.depthTestEnable(VK_TRUE);
  ds.depthWriteEnable(VK_TRUE);
  ds.depthCompareOp(vk::CompareOp::eLessOrEqual);
  ds.depthBoundsTestEnable(VK_FALSE);
  ds.stencilTestEnable(VK_FALSE);

  vk::StencilOpState stencil_state = vk::StencilOpState()
    .failOp(vk::StencilOp::eKeep)
    .passOp(vk::StencilOp::eKeep)
    .compareOp(vk::CompareOp::eAlways);
  ds.back(stencil_state);
  ds.front(stencil_state);

  ms.pSampleMask(nullptr);
  ms.rasterizationSamples(vk::SampleCountFlagBits::e1);

  // Two stages: vs and fs
  pipeline_create_info.stageCount(2);
  vk::PipelineShaderStageCreateInfo shader_stages[2];

  Shader::InitializeGlslang();
  shader_stages[0].stage(vk::ShaderStageFlagBits::eVertex);
  shader_stages[0].module(PrepareVs(device));
  shader_stages[0].pName("main");

  shader_stages[1].stage(vk::ShaderStageFlagBits::eFragment);
  shader_stages[1].module(PrepareFs(device));
  shader_stages[1].pName("main");
  Shader::FinalizeGlslang();

  pipeline_create_info.pVertexInputState(&vertexState);
  pipeline_create_info.pInputAssemblyState(&ia);
  pipeline_create_info.pRasterizationState(&rs);
  pipeline_create_info.pColorBlendState(&cb);
  pipeline_create_info.pMultisampleState(&ms);
  pipeline_create_info.pViewportState(&vp);
  pipeline_create_info.pDepthStencilState(&ds);
  pipeline_create_info.pStages(shader_stages);
  pipeline_create_info.renderPass(renderPass);
  pipeline_create_info.pDynamicState(&dynamic_state);

  vk::PipelineCache pipeline_cache;
  vk::PipelineCacheCreateInfo pipeline_cache_create_info;
  vk::chk(device.createPipelineCache(&pipeline_cache_create_info, nullptr,
                                     &pipeline_cache));

  vk::Pipeline pipeline;
  vk::chk(device.createGraphicsPipelines(pipeline_cache, 1, &pipeline_create_info,
                                         nullptr, &pipeline));

  device.destroyPipelineCache(pipeline_cache, nullptr);

  device.destroyShaderModule(shader_stages[0].module(), nullptr);
  device.destroyShaderModule(shader_stages[1].module(), nullptr);

  return pipeline;
}

void DemoScene::PrepareFramebuffers() {
  vk::ImageView attachments[2];
  attachments[1] = vk_depth_buffer().view;

  const vk::FramebufferCreateInfo fb_info =
    vk::FramebufferCreateInfo()
      .renderPass(render_pass_)
      .attachmentCount(2)
      .pAttachments(attachments)
      .width(framebuffer_size().x)
      .height(framebuffer_size().y)
      .layers(1);

  framebuffers_ = std::unique_ptr<vk::Framebuffer>{
      new vk::Framebuffer[vk_swapchain_image_count()]};

  for (uint32_t i = 0; i < vk_swapchain_image_count(); i++) {
    attachments[0] = vk_buffers()[i].view;
    vk::chk(vk_device().createFramebuffer(&fb_info, nullptr,
                                         &framebuffers_.get()[i]));
  }
}

void DemoScene::Prepare() {
  PrepareVertices();
  PrepareIndices();

  PrepareDescriptorLayout();
  PrepareUniformBuffer();
  PrepareDescriptorPool();
  PrepareDescriptorSet();

  PrepareRenderPass();
  pipeline_ = PreparePipeline(vk_device(), vertex_input_,
                              pipeline_layout_, render_pass_);

  PrepareFramebuffers();
}

void DemoScene::Cleanup() {
  for (uint32_t i = 0; i < vk_swapchain_image_count(); i++) {
    vk_device().destroyFramebuffer(framebuffers_.get()[i], nullptr);
  }
  vk_device().destroyDescriptorPool(desc_pool_, nullptr);

  vk_device().destroyPipeline(pipeline_, nullptr);
  vk_device().destroyRenderPass(render_pass_, nullptr);
  vk_device().destroyPipelineLayout(pipeline_layout_, nullptr);
  vk_device().destroyDescriptorSetLayout(desc_layout_, nullptr);

  vk_device().destroyBuffer(uniform_data_.buf, nullptr);
  vk_device().freeMemory(uniform_data_.mem, nullptr);

  vk_device().destroyBuffer(vertex_attribs_.buf, nullptr);
  vk_device().freeMemory(vertex_attribs_.mem, nullptr);
  vk_device().destroyBuffer(instance_attribs_.buf, nullptr);
  vk_device().freeMemory(instance_attribs_.mem, nullptr);
  vk_device().destroyBuffer(indices_.buf, nullptr);
  vk_device().freeMemory(indices_.mem, nullptr);
}

#include <ctime>
static clock_t startTime;
static int renderedFrames;

DemoScene::DemoScene(GLFWwindow *window)
    : VulkanScene(window)
    , quad_trees_{
        {Settings::kFaceSize, CubeFace::kPosX},
        {Settings::kFaceSize, CubeFace::kNegX},
        {Settings::kFaceSize, CubeFace::kPosY},
        {Settings::kFaceSize, CubeFace::kNegY},
        {Settings::kFaceSize, CubeFace::kPosZ},
        {Settings::kFaceSize, CubeFace::kNegZ},
      }
    // , thread_pool_{std::max(std::thread::hardware_concurrency(), 2u) - 1} {
    , thread_pool_{1} {
  Prepare();

  int radius = Settings::kSphereRadius;
  tp_camera_ = AddComponent<engine::ThirdPersonalCamera>(
        M_PI/3, 2, 3*radius, glm::vec3(-2*radius, 0, 0),
        5, 1, 0.005, 4, radius, radius);
  set_camera(tp_camera_);

  startTime = clock();
}

DemoScene::~DemoScene() {
  clock_t endTime = clock();
  double elapsedSecs = double(endTime - startTime) / CLOCKS_PER_SEC;
  std::cout << "Average FPS: " << renderedFrames / elapsedSecs << std::endl;
  Cleanup();

  for (uint32_t i = 0; i < used_index_count_; i++) {
    FreeTexture(i);
    vk_device().destroyImageView(textures_[i].view, nullptr);
    vk_device().destroyImage(textures_[i].image, nullptr);
    vk_device().freeMemory(textures_[i].mem, nullptr);
    vk_device().destroySampler(textures_[i].sampler, nullptr);
  }
  used_index_count_ = 0;
}

void DemoScene::Render() {
  Draw();
  renderedFrames++;
}

void DemoScene::Update() {
  {
    double height = glm::length(camera()->transform().pos());
    double z_far = 1000 * height, z_near = 1;
    camera()->set_z_far(z_far);
    camera()->set_z_near(z_near);
    if (free_fly_camera_) {
      auto t = free_fly_camera_->transform();
      glm::vec3 new_up = glm::normalize(t.pos());
      t.set_up(new_up);
    }
  }

  {
    UniformData* uniform_data = (UniformData*)
        vk_device().mapMemory(uniform_data_.mem, 0, sizeof(UniformData), vk::MemoryMapFlags{});

    uniform_data->projectionMatrix = scene()->camera()->projectionMatrix();
    uniform_data->cameraMatrix = scene()->camera()->cameraMatrix();
    uniform_data->camera_pos = scene()->camera()->transform().pos();
    uniform_data->terrain_smallest_geometry_lod_distance = Settings::kSmallestGeometryLodDistance;
    uniform_data->terrain_smallest_texture_lod_distance = Settings::kSmallestTextureLodDistance;
    uniform_data->terrain_sphere_radius = Settings::kSphereRadius;
    uniform_data->face_size = Settings::kFaceSize;
    uniform_data->height_scale = Settings::kMaxHeight;
    uniform_data->terrain_max_lod_level = quad_trees_[0].max_node_level();
    uniform_data->terrain_level_offset = Settings::kLevelOffset;
    uniform_data->diffuse_texture_dimension_w_borders = Settings::kDiffuseTexSizeWithBorders;
    uniform_data->elevation_texture_dimension_w_borders = Settings::kElevationTexSizeWithBorders;

    vk_device().unmapMemory(uniform_data_.mem);
  }

  // update instances to draw
  if (!freeze_scene_) {
    thread_pool_.Clear();
    grid_mesh_.ClearRenderList();
    for (CdlodQuadTree& quad_tree : quad_trees_) {
      quad_tree.SelectNodes(*scene()->camera(), grid_mesh_, thread_pool_, *this);
    }

    size_t instance_count = grid_mesh_.mesh_.attribs_.size();
    if (instance_count > Settings::kMaxInstanceCount) {
      std::cerr << "Number of instances used: " << instance_count << std::endl;
      std::terminate();
    }

    {
      PerInstanceAttributes *attribs = (PerInstanceAttributes*)
        vk_device().mapMemory(instance_attribs_.mem, 0,
                              instance_count * sizeof(PerInstanceAttributes),
                              vk::MemoryMapFlags{});

      for (int i = 0; i < instance_count; ++i) {
        attribs[i] = grid_mesh_.mesh_.attribs_[i];
      }

      vk_device().unmapMemory(instance_attribs_.mem);
    }

    vk::WriteDescriptorSet writes[1];

    writes[0].dstBinding(0);
    writes[0].dstSet(desc_set_);
    writes[0].descriptorCount(used_index_count_);
    writes[0].descriptorType(vk::DescriptorType::eCombinedImageSampler);
    writes[0].pImageInfo(tex_descriptor_infos_);

    vk_device().updateDescriptorSets(1, writes, 0, nullptr);

    //FlushInitCommand();
  }
}

void DemoScene::ScreenResizedClean() {
  Cleanup();
  VulkanScene::ScreenResizedClean();
}

void DemoScene::ScreenResized(size_t width, size_t height) {
  VulkanScene::ScreenResized(width, height);
  Prepare();
}

void DemoScene::KeyAction(int key, int scancode, int action, int mods) {
  int radius = Settings::kSphereRadius;
  if (action == GLFW_PRESS) {
    if (key == GLFW_KEY_SPACE) {
      if (free_fly_camera_) {
        glm::dvec3 pos = free_fly_camera_->transform().pos();
        std::cout << pos << std::endl;
        std::cout << pos + free_fly_camera_->transform().forward() << std::endl;
        double current_dist = glm::length(pos) - radius;
        if (current_dist < 100) {
          current_dist = radius + 100;
          pos = current_dist * glm::normalize(pos);
        }

        tp_camera_ = AddComponent<engine::ThirdPersonalCamera>(
            M_PI/3, 2, 3*radius, pos,
            5, 1, 0.005, 4, radius, radius);
        RemoveComponent(free_fly_camera_);
        free_fly_camera_ = nullptr;
        set_camera(tp_camera_);
      } else {
        free_fly_camera_ = AddComponent<engine::FreeFlyCamera>(
          M_PI/3, 2, 3*radius, tp_camera_->transform().pos(), glm::vec3(), 10, 5);
        glm::vec3 up{glm::normalize(free_fly_camera_->transform().pos())};
        free_fly_camera_->transform().set_forward(-up + glm::vec3{0, 0.25, 0});
        free_fly_camera_->transform().set_up(up);
        RemoveComponent(tp_camera_);
        tp_camera_ = nullptr;
        set_camera(free_fly_camera_);
      }
    } else if (key == GLFW_KEY_KP_9) {
      RemoveComponent(camera());
      tp_camera_ = nullptr;

      double old_radius = 27501;
      double radius = Settings::kSphereRadius;
      double scale = radius / old_radius;
      free_fly_camera_ = AddComponent<engine::FreeFlyCamera>(
          M_PI/3, 2, 3*radius, scale * glm::dvec3{-18829.6, 19925.4, 3163.6},
          scale * glm::dvec3{-18829.101141, 19925.3065359, 3164.461629}, 0, 0);
      set_camera(free_fly_camera_);
    } else if (key == GLFW_KEY_KP_8) {
      RemoveComponent(camera());
      tp_camera_ = nullptr;

      double old_radius = 27501;
      double radius = Settings::kSphereRadius;
      double scale = radius / old_radius;
      free_fly_camera_ = AddComponent<engine::FreeFlyCamera>(
          M_PI/3, 2, 3*radius, scale * glm::dvec3{-9943.69, 16404.8, 20361.8},
          scale * glm::dvec3{-9942.71, 16404.6, 20361.7}, 50, 0);
      set_camera(free_fly_camera_);
    } else if (key == GLFW_KEY_KP_7) {
      RemoveComponent(camera());
      tp_camera_ = nullptr;

      double radius = Settings::kSphereRadius;
      free_fly_camera_ = AddComponent<engine::FreeFlyCamera>(
          M_PI/3, 2, 3*radius, glm::dvec3{-22375.5, 23670.3, 3756.66},
          glm::dvec3{-22374.9, 23670.6, 3757.41}, 50, 0);
      set_camera(free_fly_camera_);
    } else if (key == GLFW_KEY_F) {
      freeze_scene_ = !freeze_scene_;
    }
  }
}

void DemoScene::MouseScrolled(double xoffset, double yoffset) {
  if (free_fly_camera_) {
    auto cam = free_fly_camera_;
    cam->set_speed_per_sec(cam->speed_per_sec() * (yoffset > 0 ? 1.1 : 0.9));
  }
}
