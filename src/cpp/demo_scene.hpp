// Copyright (c) 2016, Tamas Csala

#ifndef DEMO_SCENE_HPP_
#define DEMO_SCENE_HPP_

#include <vulkan/vk_cpp.hpp>
#include <GLFW/glfw3.h>

#include "engine/vulkan_scene.hpp"
#include "cdlod/cdlod_quad_tree.hpp"
#include "cdlod/texture_handler.hpp"
#include "common/vulkan_application.hpp"
#include "common/thread_pool.hpp"

struct TextureObject {
  vk::Sampler sampler;

  vk::Image image;
  vk::ImageLayout imageLayout;

  vk::DeviceMemory mem;
  vk::ImageView view;
  int32_t tex_width = 0, tex_height = 0;
};

struct UniformData {
  glm::mat4 cameraMatrix;
  glm::mat4 projectionMatrix;

  glm::vec3 camera_pos;
  float depthCoef;

  float terrain_smallest_geometry_lod_distance;
  float terrain_sphere_radius;
  float face_size;
  float height_scale;

  int terrain_max_lod_level;
  int terrain_level_offset;

  int texture_dimension;
  int diffuse_texture_dimension_w_borders;
  int elevation_texture_dimension_w_borders;
};

class DemoScene : public engine::VulkanScene, public TextureHandler {
public:
  DemoScene(GLFWwindow *window);
  ~DemoScene();

  virtual void Render() override;
  virtual void Update() override;
  virtual void ScreenResizedClean() override;
  virtual void ScreenResized(size_t width, size_t height) override;

private:
  struct TextureObject textures_[Settings::kMaxTextureCount];

  struct {
    vk::Buffer buf;
    vk::DeviceMemory mem;
    vk::DescriptorBufferInfo buffer_info;
  } uniform_data_;

  vk::PipelineVertexInputStateCreateInfo vertex_input_;
  vk::VertexInputBindingDescription vertex_input_bindings_[2];
  vk::VertexInputAttributeDescription vertex_input_attribs_[14];

  struct {
    vk::Buffer buf;
    vk::DeviceMemory mem;
  } vertex_attribs_, instance_attribs_, indices_;

  vk::PipelineLayout pipeline_layout_;
  vk::DescriptorSetLayout desc_layout_;
  vk::RenderPass render_pass_;
  vk::Pipeline pipeline_;

  vk::DescriptorPool desc_pool_;
  vk::DescriptorSet desc_set_;

  std::unique_ptr<vk::Framebuffer> framebuffers_;

  QuadGridMesh grid_mesh_{Settings::kNodeDimension};
  CdlodQuadTree quad_trees_[6];
  ThreadPool thread_pool_;

  void BuildDrawCmd();
  void Draw();
  void PrepareTextureImage(const unsigned char *tex_colors,
                           int tex_width, int tex_height,
                           TextureObject *tex_obj, vk::ImageTiling tiling,
                           vk::ImageUsageFlags usage,
                           vk::MemoryPropertyFlags required_props,
                           vk::Format tex_format);
  void PrepareIndices();
  void PrepareVertices();
  void PrepareDescriptorLayout();
  void PrepareRenderPass();
  void PrepareDescriptorPool();
  void PrepareUniformBuffer();
  void PrepareDescriptorSet();
  void PrepareFramebuffers();
  void Prepare();
  void Cleanup();

  void SetupTexture(size_t index, unsigned width, unsigned height,
                    const unsigned char* data) override;
  void FreeTexture(size_t index) override;
};

#endif // DEMO_SCENE_HPP_
