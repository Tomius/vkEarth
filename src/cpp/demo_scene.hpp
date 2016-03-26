#ifndef DEMO_SCENE_HPP_
#define DEMO_SCENE_HPP_

#include <vulkan/vk_cpp.h>
#include <GLFW/glfw3.h>

#include "engine/scene.hpp"
#include "cdlod/grid_mesh.hpp"
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

struct Demo {
    GLFWwindow* window;
    bool use_staging_buffer = false;

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

    vk::PipelineLayout pipeline_layout;
    vk::DescriptorSetLayout desc_layout;
    vk::RenderPass render_pass;
    vk::Pipeline pipeline;

    vk::DescriptorPool desc_pool;
    vk::DescriptorSet desc_set;

    vk::Framebuffer *framebuffers = nullptr;

    GridMesh gridMesh{64};
};

class DemoScene : public engine::Scene {
public:
  DemoScene(GLFWwindow *window);
  ~DemoScene();

  virtual void Render() override;
  virtual void Update() override;
  virtual void ScreenResizedClean() override;
  virtual void ScreenResized(size_t width, size_t height) override;

private:
  Demo demo_;
};

#endif // DEMO_SCENE_HPP_
