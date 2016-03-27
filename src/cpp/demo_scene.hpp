#ifndef DEMO_SCENE_HPP_
#define DEMO_SCENE_HPP_

#include <vulkan/vk_cpp.h>
#include <GLFW/glfw3.h>

#include "engine/scene.hpp"
#include "cdlod/cdlod_quad_tree.hpp"
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

struct UniformData {
  glm::mat4 mvp;
  glm::vec3 cameraPos;
  float terrainSmallestGeometryLodDistance;
  int terrainMaxLoadLevel;
};

struct Demo {
    GLFWwindow* window;
    bool kUseStagingBuffer = false;

    struct TextureObject textures[DEMO_TEXTURE_COUNT];

    struct {
      vk::Buffer buf;
      vk::DeviceMemory mem;
      vk::DescriptorBufferInfo bufferInfo;
    } uniformData;

    vk::PipelineVertexInputStateCreateInfo vertexInput;
    vk::VertexInputBindingDescription vertexInputBindings[2];
    vk::VertexInputAttributeDescription vertexInputAttribs[2];

    struct {
        vk::Buffer buf;
        vk::DeviceMemory mem;
    } vertexAttribs, instanceAttribs, indices;

    vk::PipelineLayout pipelineLayout;
    vk::DescriptorSetLayout descLayout;
    vk::RenderPass renderPass;
    vk::Pipeline pipeline;

    vk::DescriptorPool descPool;
    vk::DescriptorSet descSet;

    vk::Framebuffer *framebuffers = nullptr;

    QuadGridMesh gridMesh{Settings::kNodeDimension};
    CdlodQuadTree quadTree{Settings::kFaceSize, CubeFace::kPosX};
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
