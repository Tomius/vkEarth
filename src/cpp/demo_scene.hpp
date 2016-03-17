#ifndef DEMO_SCENE_HPP_
#define DEMO_SCENE_HPP_

#include <vulkan/vk_cpp.h>
#include <GLFW/glfw3.h>

#include "engine/scene.hpp"
#include "initialize/debug_callback.hpp"
#include "common/vulkan_application.hpp"

#define DEMO_TEXTURE_COUNT 1

struct TextureObject {
    vk::Sampler sampler;

    vk::Image image;
    vk::ImageLayout imageLayout;

    vk::DeviceMemory mem;
    vk::ImageView view;
    int32_t tex_width = 0, tex_height = 0;
};

struct SwapchainBuffers {
    vk::Image image;
    vk::CommandBuffer cmd;
    vk::ImageView view;
};

struct Demo {
    GLFWwindow* window;
    VkSurfaceKHR surface;
    bool use_staging_buffer = false;

    vk::Instance inst;
    std::unique_ptr<Initialize::DebugCallback> debugCallback;
    vk::PhysicalDevice gpu;
    vk::Device device;
    vk::Queue queue;
    uint32_t graphicsQueueNodeIndex = 0;

    int width = 600, height = 600;
    vk::Format format;
    vk::ColorSpaceKHR colorSpace;

    uint32_t swapchainImageCount = 0;
    vk::SwapchainKHR swapchain;
    SwapchainBuffers *buffers = nullptr;

    vk::CommandPool cmd_pool;

    struct {
        vk::Format format;

        vk::Image image;
        vk::DeviceMemory mem;
        vk::ImageView view;
    } depth;

    struct TextureObject textures[DEMO_TEXTURE_COUNT];

    struct {
      vk::Buffer buf;
      vk::DeviceMemory mem;
      vk::DescriptorBufferInfo bufferInfo;
    } uniformData;

    struct {
        vk::Buffer buf;
        vk::DeviceMemory mem;

        vk::PipelineVertexInputStateCreateInfo vi;
        vk::VertexInputBindingDescription vi_bindings[1];
        vk::VertexInputAttributeDescription vi_attrs[2];
    } vertices;

    struct {
        vk::Buffer buf;
        vk::DeviceMemory mem;
    } indices;

    vk::CommandBuffer setup_cmd; // Command Buffer for initialization commands
    vk::CommandBuffer draw_cmd;  // Command Buffer for drawing commands
    vk::PipelineLayout pipeline_layout;
    vk::DescriptorSetLayout desc_layout;
    vk::RenderPass render_pass;
    vk::Pipeline pipeline;

    vk::DescriptorPool desc_pool;
    vk::DescriptorSet desc_set;

    vk::Framebuffer *framebuffers = nullptr;

    vk::PhysicalDeviceMemoryProperties memoryProperties;

    VulkanApplication app;

    uint32_t current_buffer = 0;
};

class DemoScene : public engine::Scene {
public:
  DemoScene(GLFWwindow *window);
  ~DemoScene();

  virtual void render() override;
  virtual void update() override;
  virtual void screenResized(size_t width, size_t height) override;

private:
  Demo demo_;
};

#endif // DEMO_SCENE_HPP_
