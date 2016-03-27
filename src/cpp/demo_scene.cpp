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

#include "engine/scene.hpp"
#include "common/error_checking.hpp"
#include "initialize/create_pipeline.hpp"
#include "common/vulkan_memory.hpp"

#define VERTEX_BUFFER_BIND_ID 0
#define INSTANCE_BUFFER_BIND_ID 1

#if defined(NDEBUG) && defined(__GNUC__)
  #define U_ASSERT_ONLY __attribute__((unused))
#else
  #define U_ASSERT_ONLY
#endif

static void demo_draw_build_cmd(Demo *demo, engine::Scene& scene) {
    const vk::CommandBufferInheritanceInfo cmd_buf_hinfo;
    const vk::CommandBufferBeginInfo cmd_buf_info =
        vk::CommandBufferBeginInfo().pInheritanceInfo(&cmd_buf_hinfo);
    const vk::ClearValue clear_values[2] = {
        vk::ClearValue().color(
            vk::ClearColorValue{std::array<float,4>{0.2f, 0.2f, 0.2f, 0.2f}}),
        vk::ClearValue().depthStencil(vk::ClearDepthStencilValue{1.0, 0})
    };
    const vk::RenderPassBeginInfo rp_begin = vk::RenderPassBeginInfo()
        .renderPass(demo->renderPass)
        .framebuffer(demo->framebuffers[scene.vkCurrentBuffer()])
        .renderArea({vk::Offset2D(0, 0),
            vk::Extent2D(scene.framebufferSize().x, scene.framebufferSize().y)})
        .clearValueCount(2)
        .pClearValues(clear_values);
    vk::Result U_ASSERT_ONLY err;

    err = scene.vkDrawCmd().begin(&cmd_buf_info);
    assert(err == vk::Result::eSuccess);

    scene.vkDrawCmd().beginRenderPass(&rp_begin, vk::SubpassContents::eInline);
    scene.vkDrawCmd().bindPipeline(vk::PipelineBindPoint::eGraphics, demo->pipeline);
    scene.vkDrawCmd().bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
        demo->pipelineLayout, 0, 1, &demo->descSet, 0, nullptr);

    vk::Viewport viewport = vk::Viewport()
      .width(scene.framebufferSize().x)
      .height(scene.framebufferSize().y)
      .minDepth(0.0f)
      .maxDepth(1.0f);
    scene.vkDrawCmd().setViewport(0, 1, &viewport);

    vk::Rect2D scissor{vk::Offset2D(0, 0), vk::Extent2D(scene.framebufferSize().x, scene.framebufferSize().y)};
    scene.vkDrawCmd().setScissor(0, 1, &scissor);

    vk::DeviceSize offsets[1] = {0};
    scene.vkDrawCmd().bindVertexBuffers(VERTEX_BUFFER_BIND_ID, 1,
                                        &demo->vertexAttribs.buf, offsets);
    scene.vkDrawCmd().bindVertexBuffers(INSTANCE_BUFFER_BIND_ID, 1,
                                        &demo->instanceAttribs.buf, offsets);
    scene.vkDrawCmd().bindIndexBuffer(demo->indices.buf,
                                      vk::DeviceSize{},
                                      vk::IndexType::eUint16);

    scene.vkDrawCmd().setLineWidth(2.0f);

    // scene.vkDrawCmd().draw(6, 1, 0, 0);
    scene.vkDrawCmd().drawIndexed(demo->gridMesh.mesh_.index_count_,
                                  demo->gridMesh.mesh_.renderData_.size(), 0, 0, 0);
    scene.vkDrawCmd().endRenderPass();

    vk::ImageMemoryBarrier prePresentBarrier = vk::ImageMemoryBarrier()
        .srcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
        .dstAccessMask(vk::AccessFlagBits::eMemoryRead)
        .oldLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .newLayout(vk::ImageLayout::ePresentSrcKHR)
        .srcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .dstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .subresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

    prePresentBarrier.image(scene.vkBuffers()[scene.vkCurrentBuffer()].image);
    vk::ImageMemoryBarrier *pmemory_barrier = &prePresentBarrier;
    scene.vkDrawCmd().pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                                   vk::PipelineStageFlagBits::eBottomOfPipe,
                                   vk::DependencyFlags(), 0, nullptr, 0,
                                   nullptr, 1, pmemory_barrier);

    err = scene.vkDrawCmd().end();
    assert(err == vk::Result::eSuccess);
}

static void demo_draw(Demo *demo, engine::Scene& scene) {
    vk::Result U_ASSERT_ONLY err;
    VkResult vkErr;
    vk::Semaphore presentCompleteSemaphore;
    vk::SemaphoreCreateInfo presentCompleteSemaphoreCreateInfo;

    err = scene.vkDevice().createSemaphore(&presentCompleteSemaphoreCreateInfo,
                                       nullptr, &presentCompleteSemaphore);
    assert(err == vk::Result::eSuccess);

    // Get the index of the next available swapchain image:
    vkErr = scene.vkApp().entryPoints.fpAcquireNextImageKHR(
      scene.vkDevice(), scene.vkSwapchain(), UINT64_MAX, presentCompleteSemaphore,
      (vk::Fence)nullptr, &scene.vkCurrentBuffer());
    if (vkErr == VK_ERROR_OUT_OF_DATE_KHR) {
        // scene.vkSwapchain() is out of date (e.g. the window was resized) and
        // must be recreated:
        //demo_resize(demo, scene);
        throw std::runtime_error("Should resize swapchain");
        demo_draw(demo, scene);
        scene.vkDevice().destroySemaphore(presentCompleteSemaphore, nullptr);
        return;
    } else if (vkErr == VK_SUBOPTIMAL_KHR) {
        // scene.vkSwapchain() is not as optimal as it could be, but the platform's
        // presentation engine will still present the image correctly.
    } else {
        assert(vkErr == VK_SUCCESS);
    }

    // Assume the command buffer has been run on current_buffer before so
    // we need to set the image layout back to COLOR_ATTACHMENT_OPTIMAL
    scene.SetImageLayout(scene.vkBuffers()[scene.vkCurrentBuffer()].image,
                         vk::ImageAspectFlagBits::eColor,
                         vk::ImageLayout::ePresentSrcKHR,
                         vk::ImageLayout::eColorAttachmentOptimal);
    scene.FlushInitCommand();

    // Wait for the present complete semaphore to be signaled to ensure
    // that the image won't be rendered to until the presentation
    // engine has fully released ownership to the application, and it is
    // okay to render to the image.

    // FIXME/TODO: DEAL WITH vk::ImageLayout::ePresentSrcKHR
    demo_draw_build_cmd(demo, scene);
    vk::Fence nullFence;
    vk::PipelineStageFlags pipe_stage_flags =
        vk::PipelineStageFlagBits::eBottomOfPipe;
    vk::SubmitInfo submit_info = vk::SubmitInfo()
        .waitSemaphoreCount(1)
        .pWaitSemaphores(&presentCompleteSemaphore)
        .pWaitDstStageMask(&pipe_stage_flags)
        .commandBufferCount(1)
        .pCommandBuffers(&scene.vkDrawCmd());

    err = scene.vkQueue().submit(1, &submit_info, nullFence);
    assert(err == vk::Result::eSuccess);

    vk::PresentInfoKHR present = vk::PresentInfoKHR()
        .swapchainCount(1)
        .pSwapchains(&scene.vkSwapchain())
        .pImageIndices(&scene.vkCurrentBuffer());
    VkPresentInfoKHR& vkPresent =
      const_cast<VkPresentInfoKHR&>(
        static_cast<const VkPresentInfoKHR&>(present));

    // TBD/TODO: SHOULD THE "present" PARAMETER BE "const" IN THE HEADER?
    vkErr = scene.vkApp().entryPoints.fpQueuePresentKHR(scene.vkQueue(), &vkPresent);
    if (vkErr == VK_ERROR_OUT_OF_DATE_KHR) {
        // scene.vkSwapchain() is out of date (e.g. the window was resized) and
        // must be recreated:
        //demo_resize(demo, scene);
        throw std::runtime_error("Should resize swapchain");
    } else if (vkErr == VK_SUBOPTIMAL_KHR) {
        // scene.vkSwapchain() is not as optimal as it could be, but the platform's
        // presentation engine will still present the image correctly.
    } else {
        assert(vkErr == VK_SUCCESS);
    }

    err = scene.vkQueue().waitIdle();
    assert(err == vk::Result::eSuccess);

    scene.vkDevice().destroySemaphore(presentCompleteSemaphore, nullptr);
}

static void
demo_prepare_texture_image(Demo *demo, engine::Scene& scene,
                           const uint32_t *tex_colors,
                           TextureObject *tex_obj, vk::ImageTiling tiling,
                           vk::ImageUsageFlags usage,
                           vk::MemoryPropertyFlags required_props) {
    const vk::Format tex_format = vk::Format::eB8G8R8A8Unorm;
    const int32_t tex_width = 2;
    const int32_t tex_height = 2;
    vk::Result U_ASSERT_ONLY err;

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

    err = scene.vkDevice().createImage(&image_create_info, nullptr, &tex_obj->image);
    assert(err == vk::Result::eSuccess);

    scene.vkDevice().getImageMemoryRequirements(tex_obj->image, &mem_reqs);

    mem_alloc.allocationSize(mem_reqs.size());
    MemoryTypeFromProperties(scene.vkGpuMemoryProperties(),
                             mem_reqs.memoryTypeBits(),
                             required_props, mem_alloc);

    /* allocate memory */
    err = scene.vkDevice().allocateMemory(&mem_alloc, nullptr, &tex_obj->mem);
    assert(err == vk::Result::eSuccess);

    /* bind memory */
    err = scene.vkDevice().bindImageMemory(tex_obj->image, tex_obj->mem, 0);
    assert(err == vk::Result::eSuccess);

    if (required_props & vk::MemoryPropertyFlagBits::eHostVisible) {
        const vk::ImageSubresource subres =
          vk::ImageSubresource().aspectMask(vk::ImageAspectFlagBits::eColor);
        vk::SubresourceLayout layout;
        void *data;

        scene.vkDevice().getImageSubresourceLayout(tex_obj->image, &subres, &layout);

        err = scene.vkDevice().mapMemory(tex_obj->mem, 0, mem_alloc.allocationSize(),
                                     vk::MemoryMapFlags(), &data);
        assert(err == vk::Result::eSuccess);

        for (int32_t y = 0; y < tex_height; y++) {
            uint32_t *row = (uint32_t *)((char *)data + layout.rowPitch() * y);
            for (int32_t x = 0; x < tex_width; x++)
                row[x] = tex_colors[(x & 1) ^ (y & 1)];
        }

        scene.vkDevice().unmapMemory(tex_obj->mem);
    }

    tex_obj->imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    scene.SetImageLayout(tex_obj->image, vk::ImageAspectFlagBits::eColor,
                         vk::ImageLayout::eUndefined, tex_obj->imageLayout);
    /* setting the image layout does not reference the actual memory so no need
     * to add a mem ref */
}

static void demo_destroy_texture_image(Demo *demo, engine::Scene& scene,
                                       TextureObject *tex_obj) {
    /* clean up staging resources */
    scene.vkDevice().destroyImage(tex_obj->image, nullptr);
    scene.vkDevice().freeMemory(tex_obj->mem, nullptr);
}

static void demo_prepare_textures(Demo *demo, engine::Scene& scene) {
    const vk::Format tex_format = vk::Format::eB8G8R8A8Unorm;
    vk::FormatProperties props;
    const uint32_t tex_colors[DEMO_TEXTURE_COUNT][2] = {
        {0xffff0000, 0xff00ff00},
    };
    vk::Result U_ASSERT_ONLY err;

    scene.vkGpu().getFormatProperties(tex_format, &props);

    for (uint32_t i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        if ((props.linearTilingFeatures() &
             vk::FormatFeatureFlagBits::eSampledImage) &&
            !demo->kUseStagingBuffer) {
            /* Device can texture using linear textures */
            demo_prepare_texture_image(demo, scene, tex_colors[i], &demo->textures[i],
                                       vk::ImageTiling::eLinear,
                                       vk::ImageUsageFlagBits::eSampled,
                                       vk::MemoryPropertyFlagBits::eHostVisible);
        } else if (props.optimalTilingFeatures() &
                   vk::FormatFeatureFlagBits::eSampledImage) {
            /* Must use staging buffer to copy linear texture to optimized */
            TextureObject staging_texture;

            demo_prepare_texture_image(demo, scene, tex_colors[i], &staging_texture,
                                       vk::ImageTiling::eLinear,
                                       vk::ImageUsageFlagBits::eTransferSrc,
                                       vk::MemoryPropertyFlagBits::eHostVisible);

            demo_prepare_texture_image(
                demo, scene, tex_colors[i], &demo->textures[i],
                vk::ImageTiling::eOptimal,
                (vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled),
                vk::MemoryPropertyFlagBits::eDeviceLocal);

            scene.SetImageLayout(staging_texture.image,
                                 vk::ImageAspectFlagBits::eColor,
                                 staging_texture.imageLayout,
                                 vk::ImageLayout::eTransferSrcOptimal);

            scene.SetImageLayout(demo->textures[i].image,
                                 vk::ImageAspectFlagBits::eColor,
                                 demo->textures[i].imageLayout,
                                 vk::ImageLayout::eTransferDstOptimal);

            vk::ImageCopy copy_region = vk::ImageCopy()
                .srcSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
                .dstSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
                .extent(vk::Extent3D(staging_texture.tex_width,
                                     staging_texture.tex_height, 1));
            scene.vkSetupCmd().copyImage(staging_texture.image,
                vk::ImageLayout::eTransferSrcOptimal, demo->textures[i].image,
                vk::ImageLayout::eTransferDstOptimal, 1, &copy_region);

            scene.SetImageLayout(demo->textures[i].image,
                                 vk::ImageAspectFlagBits::eColor,
                                 vk::ImageLayout::eTransferDstOptimal,
                                 demo->textures[i].imageLayout);

            scene.FlushInitCommand();

            demo_destroy_texture_image(demo, scene, &staging_texture);
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
        err = scene.vkDevice().createSampler(&sampler, nullptr,
                                         &demo->textures[i].sampler);
        assert(err == vk::Result::eSuccess);

        /* create image view */
        view.image(demo->textures[i].image);
        err = scene.vkDevice().createImageView(&view, nullptr, &demo->textures[i].view);
        assert(err == vk::Result::eSuccess);
    }
}

static void demo_prepare_indices(Demo *demo, engine::Scene& scene,
                                 const std::vector<uint16_t>& indices) {
  // const GLushort indices[] = {
  //   0, 1, 2, 2, 1, 3
  // };
  const vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
      .size(sizeof(uint16_t) * indices.size())
      .usage(vk::BufferUsageFlagBits::eIndexBuffer);

  vk::MemoryAllocateInfo mem_alloc;
  vk::MemoryRequirements mem_reqs;
  vk::Result U_ASSERT_ONLY err;
  void *data;

  err = scene.vkDevice().createBuffer(&buf_info, nullptr, &demo->indices.buf);
  assert(err == vk::Result::eSuccess);

  scene.vkDevice().getBufferMemoryRequirements(demo->indices.buf, &mem_reqs);
  assert(err == vk::Result::eSuccess);

  mem_alloc.allocationSize(mem_reqs.size());
  MemoryTypeFromProperties(scene.vkGpuMemoryProperties(),
                           mem_reqs.memoryTypeBits(),
                           vk::MemoryPropertyFlagBits::eHostVisible,
                           mem_alloc);

  err = scene.vkDevice().allocateMemory(&mem_alloc, nullptr, &demo->indices.mem);
  assert(err == vk::Result::eSuccess);

  err = scene.vkDevice().mapMemory(demo->indices.mem, 0, mem_alloc.allocationSize(),
                               vk::MemoryMapFlags{}, &data);
  assert(err == vk::Result::eSuccess);

  std::memcpy(data, indices.data(), sizeof(uint16_t) * indices.size());

  scene.vkDevice().unmapMemory(demo->indices.mem);

  err = scene.vkDevice().bindBufferMemory(demo->indices.buf, demo->indices.mem, 0);
  assert(err == vk::Result::eSuccess);
}

static void demo_prepare_vertices(Demo *demo, engine::Scene& scene) {
  vk::MemoryAllocateInfo mem_alloc;
  vk::MemoryRequirements mem_reqs;

  { // vertexAttribs
    const vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
      .size(sizeof(svec2) * demo->gridMesh.mesh_.positions_.size())
      .usage(vk::BufferUsageFlagBits::eVertexBuffer);

    vk::chk(scene.vkDevice().createBuffer(&buf_info, nullptr, &demo->vertexAttribs.buf));

    scene.vkDevice().getBufferMemoryRequirements(demo->vertexAttribs.buf, &mem_reqs);

    mem_alloc.allocationSize(mem_reqs.size());
    MemoryTypeFromProperties(scene.vkGpuMemoryProperties(),
                             mem_reqs.memoryTypeBits(),
                             vk::MemoryPropertyFlagBits::eHostVisible,
                             mem_alloc);

    vk::chk(scene.vkDevice().allocateMemory(&mem_alloc, nullptr, &demo->vertexAttribs.mem));
    vk::chk(scene.vkDevice().bindBufferMemory(demo->vertexAttribs.buf, demo->vertexAttribs.mem, 0));

    void *data;
    vk::chk(scene.vkDevice().mapMemory(demo->vertexAttribs.mem, 0, mem_alloc.allocationSize(),
                                 vk::MemoryMapFlags{}, &data));

    std::memcpy(data, demo->gridMesh.mesh_.positions_.data(),
                sizeof(svec2) * demo->gridMesh.mesh_.positions_.size());

    scene.vkDevice().unmapMemory(demo->vertexAttribs.mem);
  }

  { // instanceAttribs
    const vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
      .size(sizeof(glm::vec4) * MAX_INSTANCE_COUNT)
      .usage(vk::BufferUsageFlagBits::eVertexBuffer);

    vk::chk(scene.vkDevice().createBuffer(&buf_info, nullptr, &demo->instanceAttribs.buf));

    scene.vkDevice().getBufferMemoryRequirements(demo->instanceAttribs.buf, &mem_reqs);

    mem_alloc.allocationSize(mem_reqs.size());
    MemoryTypeFromProperties(scene.vkGpuMemoryProperties(),
                             mem_reqs.memoryTypeBits(),
                             vk::MemoryPropertyFlagBits::eHostVisible,
                             mem_alloc);

    vk::chk(scene.vkDevice().allocateMemory(&mem_alloc, nullptr, &demo->instanceAttribs.mem));
    vk::chk(scene.vkDevice().bindBufferMemory(demo->instanceAttribs.buf, demo->instanceAttribs.mem, 0));
  }

  demo->vertexInput.vertexBindingDescriptionCount(2);
  demo->vertexInput.pVertexBindingDescriptions(demo->vertexInputBindings);
  demo->vertexInput.vertexAttributeDescriptionCount(2);
  demo->vertexInput.pVertexAttributeDescriptions(demo->vertexInputAttribs);

  demo->vertexInputBindings[0].binding(VERTEX_BUFFER_BIND_ID);
  demo->vertexInputBindings[0].stride(sizeof(svec2));
  demo->vertexInputBindings[0].inputRate(vk::VertexInputRate::eVertex);

  demo->vertexInputAttribs[0].binding(VERTEX_BUFFER_BIND_ID);
  demo->vertexInputAttribs[0].location(0);
  demo->vertexInputAttribs[0].format(vk::Format::eR16G16Sint);
  demo->vertexInputAttribs[0].offset(0);

  demo->vertexInputBindings[1].binding(INSTANCE_BUFFER_BIND_ID);
  demo->vertexInputBindings[1].stride(sizeof(glm::vec4));
  demo->vertexInputBindings[1].inputRate(vk::VertexInputRate::eInstance);

  demo->vertexInputAttribs[1].binding(INSTANCE_BUFFER_BIND_ID);
  demo->vertexInputAttribs[1].location(1);
  demo->vertexInputAttribs[1].format(vk::Format::eR32G32B32A32Sfloat);
  demo->vertexInputAttribs[1].offset(0);
}

static void demo_prepare_descriptor_layout(Demo *demo, engine::Scene& scene) {
    const vk::DescriptorSetLayoutBinding layout_binding[2] = {
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

    vk::Result U_ASSERT_ONLY err;

    err = scene.vkDevice().createDescriptorSetLayout(&descriptor_layout, nullptr,
                                                 &demo->descLayout);
    assert(err == vk::Result::eSuccess);

    const vk::PipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
      vk::PipelineLayoutCreateInfo()
        .setLayoutCount(1)
        .pSetLayouts(&demo->descLayout);

    err = scene.vkDevice().createPipelineLayout(&pPipelineLayoutCreateInfo, nullptr,
                                            &demo->pipelineLayout);
    assert(err == vk::Result::eSuccess);
}

static void demo_prepare_renderPass(Demo *demo, engine::Scene& scene) {
    const vk::AttachmentDescription attachments[2] = {
      vk::AttachmentDescription()
        .format(scene.vkSurfaceFormat())
        .samples(vk::SampleCountFlagBits::e1)
        .loadOp(vk::AttachmentLoadOp::eClear)
        .storeOp(vk::AttachmentStoreOp::eStore)
        .stencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .stencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .initialLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .finalLayout(vk::ImageLayout::eColorAttachmentOptimal),
      vk::AttachmentDescription()
        .format(scene.vkDepthBuffer().format)
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
    vk::Result U_ASSERT_ONLY err;

    err = scene.vkDevice().createRenderPass(&rp_info, nullptr, &demo->renderPass);
    assert(err == vk::Result::eSuccess);
}

static void demo_prepare_descriptor_pool(Demo *demo, engine::Scene& scene) {
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

    vk::Result U_ASSERT_ONLY err;
    err = scene.vkDevice().createDescriptorPool(&descriptor_pool, nullptr,
                                            &demo->descPool);
    assert(err == vk::Result::eSuccess);
}

static void init_uniform_buffer(Demo *demo, engine::Scene& scene) {
    vk::BufferCreateInfo buf_info = vk::BufferCreateInfo{}
        .usage(vk::BufferUsageFlagBits::eUniformBuffer)
        .size(sizeof(UniformData))
        .sharingMode(vk::SharingMode::eExclusive);
    vk::chk(scene.vkDevice().createBuffer(&buf_info, NULL, &demo->uniformData.buf));

    vk::MemoryRequirements mem_reqs;
    scene.vkDevice().getBufferMemoryRequirements(demo->uniformData.buf, &mem_reqs);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.allocationSize(mem_reqs.size());
    MemoryTypeFromProperties(scene.vkGpuMemoryProperties(),
                             mem_reqs.memoryTypeBits(),
                             vk::MemoryPropertyFlagBits::eHostVisible,
                             alloc_info);

    vk::chk(scene.vkDevice().allocateMemory(&alloc_info, NULL,
                                            &demo->uniformData.mem));

    vk::chk(scene.vkDevice().bindBufferMemory(demo->uniformData.buf,
                                              demo->uniformData.mem, 0));

    demo->uniformData.bufferInfo.buffer(demo->uniformData.buf);
    demo->uniformData.bufferInfo.offset(0);
    demo->uniformData.bufferInfo.range(sizeof(UniformData));
}

static void demo_prepare_descriptor_set(Demo *demo, engine::Scene& scene) {
    vk::DescriptorImageInfo tex_descs[DEMO_TEXTURE_COUNT];
    vk::WriteDescriptorSet writes[2];
    vk::Result U_ASSERT_ONLY err;

    vk::DescriptorSetAllocateInfo alloc_info =
      vk::DescriptorSetAllocateInfo()
        .descriptorPool(demo->descPool)
        .descriptorSetCount(1)
        .pSetLayouts(&demo->descLayout);
    err = scene.vkDevice().allocateDescriptorSets(&alloc_info, &demo->descSet);
    assert(err == vk::Result::eSuccess);

    for (uint32_t i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        tex_descs[i].sampler(demo->textures[i].sampler);
        tex_descs[i].imageView(demo->textures[i].view);
        tex_descs[i].imageLayout(vk::ImageLayout::eGeneral);
    }

    writes[0].dstBinding(0);
    writes[0].dstSet(demo->descSet);
    writes[0].descriptorCount(DEMO_TEXTURE_COUNT);
    writes[0].descriptorType(vk::DescriptorType::eCombinedImageSampler);
    writes[0].pImageInfo(tex_descs);

    writes[1].dstBinding(1);
    writes[1].dstSet(demo->descSet);
    writes[1].descriptorCount(1);
    writes[1].descriptorType(vk::DescriptorType::eUniformBuffer);
    writes[1].pBufferInfo(&demo->uniformData.bufferInfo);

    scene.vkDevice().updateDescriptorSets(2, writes, 0, nullptr);
}

static void demo_prepare_framebuffers(Demo *demo, engine::Scene& scene) {
    vk::ImageView attachments[2];
    attachments[1] = scene.vkDepthBuffer().view;

    const vk::FramebufferCreateInfo fb_info =
      vk::FramebufferCreateInfo()
        .renderPass(demo->renderPass)
        .attachmentCount(2)
        .pAttachments(attachments)
        .width(scene.framebufferSize().x)
        .height(scene.framebufferSize().y)
        .layers(1);
    vk::Result U_ASSERT_ONLY err;

    demo->framebuffers = new vk::Framebuffer[scene.vkSwapchainImageCount()];

    for (uint32_t i = 0; i < scene.vkSwapchainImageCount(); i++) {
        attachments[0] = scene.vkBuffers()[i].view;
        err = scene.vkDevice().createFramebuffer(&fb_info, nullptr,
                                             &demo->framebuffers[i]);
        assert(err == vk::Result::eSuccess);
    }
}

static void demo_prepare(Demo *demo, engine::Scene& scene) {
    demo_prepare_textures(demo, scene);
    demo_prepare_vertices(demo, scene);
    demo_prepare_indices(demo, scene, demo->gridMesh.mesh_.indices_);

    demo_prepare_descriptor_layout(demo, scene);
    init_uniform_buffer(demo, scene);
    demo_prepare_descriptor_pool(demo, scene);
    demo_prepare_descriptor_set(demo, scene);

    demo_prepare_renderPass(demo, scene);
    demo->pipeline = Initialize::PreparePipeline(scene.vkDevice(), demo->vertexInput,
                                                 demo->pipelineLayout, demo->renderPass);

    demo_prepare_framebuffers(demo, scene);
}

static void demo_cleanup(Demo *demo, engine::Scene& scene) {
    for (uint32_t i = 0; i < scene.vkSwapchainImageCount(); i++) {
        scene.vkDevice().destroyFramebuffer(demo->framebuffers[i], nullptr);
    }
    delete[] demo->framebuffers;
    scene.vkDevice().destroyDescriptorPool(demo->descPool, nullptr);

    scene.vkDevice().destroyPipeline(demo->pipeline, nullptr);
    scene.vkDevice().destroyRenderPass(demo->renderPass, nullptr);
    scene.vkDevice().destroyPipelineLayout(demo->pipelineLayout, nullptr);
    scene.vkDevice().destroyDescriptorSetLayout(demo->descLayout, nullptr);

    scene.vkDevice().destroyBuffer(demo->uniformData.buf, nullptr);
    scene.vkDevice().freeMemory(demo->uniformData.mem, nullptr);

    scene.vkDevice().destroyBuffer(demo->vertexAttribs.buf, nullptr);
    scene.vkDevice().freeMemory(demo->vertexAttribs.mem, nullptr);
    scene.vkDevice().destroyBuffer(demo->instanceAttribs.buf, nullptr);
    scene.vkDevice().freeMemory(demo->instanceAttribs.mem, nullptr);
    scene.vkDevice().destroyBuffer(demo->indices.buf, nullptr);
    scene.vkDevice().freeMemory(demo->indices.mem, nullptr);

    for (uint32_t i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        scene.vkDevice().destroyImageView(demo->textures[i].view, nullptr);
        scene.vkDevice().destroyImage(demo->textures[i].image, nullptr);
        scene.vkDevice().freeMemory(demo->textures[i].mem, nullptr);
        scene.vkDevice().destroySampler(demo->textures[i].sampler, nullptr);
    }
}

#include <ctime>
static clock_t startTime;
static int renderedFrames;

DemoScene::DemoScene(GLFWwindow *window) : Scene(window) {
  demo_.window = window;
  demo_prepare(&demo_, *this);
  setCamera(AddComponent<engine::FreeFlyCamera>(glm::radians(60.0), 1, 100000, glm::dvec3(0, 10, 0), glm::dvec3{10, 0, 10}, 10));
  startTime = clock();
}

DemoScene::~DemoScene() {
  demo_cleanup(&demo_, *this);
  clock_t endTime = clock();
  double elapsedSecs = double(endTime - startTime) / CLOCKS_PER_SEC;
  std::cout << "Average FPS: " << renderedFrames / elapsedSecs << std::endl;
}

void DemoScene::Render() {
  demo_draw(&demo_, *this);
  renderedFrames++;
}

void DemoScene::Update() {
  {
    glm::mat4 mvp = scene()->camera()->projectionMatrix() *
                    scene()->camera()->cameraMatrix();

    UniformData* uniformData;
    vk::chk(vkDevice().mapMemory(demo_.uniformData.mem, 0, sizeof(mvp),
                                 vk::MemoryMapFlags{}, (void **)&uniformData));

    uniformData->mvp = mvp;
    uniformData->cameraPos = scene()->camera()->transform()->pos();
    uniformData->terrainSmallestGeometryLodDistance = Settings::kSmallestGeometryLodDistance;
    uniformData->terrainMaxLoadLevel = demo_.quadTree.maxNodeLevel();

    vkDevice().unmapMemory(demo_.uniformData.mem);
  }

  // update instances to draw
  demo_.gridMesh.clearRenderList();
  demo_.quadTree.SelectNodes(*scene()->camera(), demo_.gridMesh);

  if (demo_.gridMesh.mesh_.renderData_.size() > MAX_INSTANCE_COUNT) {
    std::cerr << "Number of instances used: " << demo_.gridMesh.mesh_.renderData_.size() << std::endl;
    std::terminate();
  }

  {
    glm::vec4 *renderData;
    vk::chk(vkDevice().mapMemory(demo_.instanceAttribs.mem, 0,
                                 MAX_INSTANCE_COUNT * sizeof(glm::vec4),
                                 vk::MemoryMapFlags{}, (void **)&renderData));

    for (int i = 0; i < demo_.gridMesh.mesh_.renderData_.size(); ++i) {
      renderData[i] = demo_.gridMesh.mesh_.renderData_[i];
    }

    vkDevice().unmapMemory(demo_.instanceAttribs.mem);
  }
}


void DemoScene::ScreenResizedClean() {
  demo_cleanup(&demo_, *this);
  Scene::ScreenResizedClean();
}

void DemoScene::ScreenResized(size_t width, size_t height) {
  Scene::ScreenResized(width, height);
  demo_prepare(&demo_, *this);
}

