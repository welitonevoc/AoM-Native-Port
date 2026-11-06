#pragma once

/// @file renderer.h  
/// @brief Abstract renderer interface for AoM Native Port
///
/// Provides a platform-agnostic rendering API that can be implemented
/// with OpenGL ES 3.2 or Vulkan on Android.

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace aom {

// Forward declarations
struct DdtTexture;

/// Handle types for GPU resources
using TextureHandle = uint32_t;
using ShaderHandle = uint32_t;
using MeshHandle = uint32_t;
using BufferHandle = uint32_t;

constexpr TextureHandle INVALID_TEXTURE = 0;
constexpr ShaderHandle INVALID_SHADER = 0;
constexpr MeshHandle INVALID_MESH = 0;

/// Vertex format for AoM models
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
    glm::vec4 color;   ///< Vertex color (used by terrain)
};

/// Vertex format for terrain
struct TerrainVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

/// Vertex format for UI
struct UIVertex {
    glm::vec2 position;
    glm::vec2 texcoord;
    uint32_t color; ///< RGBA packed
};

/// Render state flags
enum class RenderState : uint32_t {
    DEPTH_TEST    = 1 << 0,
    DEPTH_WRITE   = 1 << 1,
    ALPHA_BLEND   = 1 << 2,
    CULL_BACK     = 1 << 3,
    CULL_FRONT    = 1 << 4,
    WIREFRAME     = 1 << 5,
};

/// Blend modes
enum class BlendMode {
    OPAQUE,
    ALPHA_BLEND,
    ADDITIVE,
    MULTIPLY,
};

/// Camera for scene rendering
struct Camera {
    glm::vec3 position{0, 10, 10};
    glm::vec3 target{0, 0, 0};
    glm::vec3 up{0, 1, 0};
    float fov = 45.0f;           ///< Field of view in degrees
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    float aspect = 16.0f / 9.0f;

    [[nodiscard]] glm::mat4 view_matrix() const {
        return glm::lookAt(position, target, up);
    }

    [[nodiscard]] glm::mat4 projection_matrix() const {
        return glm::perspective(glm::radians(fov), aspect, near_plane, far_plane);
    }

    [[nodiscard]] glm::mat4 view_projection() const {
        return projection_matrix() * view_matrix();
    }
};

/// Directional light (sun)
struct DirectionalLight {
    glm::vec3 direction{-0.5f, -1.0f, -0.3f};
    glm::vec3 color{1.0f, 0.95f, 0.9f};
    float intensity = 1.0f;
    glm::vec3 ambient{0.3f, 0.3f, 0.35f};
};

/// Abstract renderer interface
class IRenderer {
public:
    virtual ~IRenderer() = default;

    /// Initialize the renderer with a native window
    /// @param native_window Platform-specific window handle (ANativeWindow* on Android)
    /// @param width Initial width in pixels
    /// @param height Initial height in pixels
    /// @return true on success
    virtual bool initialize(void* native_window, int width, int height) = 0;

    /// Shutdown and release all resources
    virtual void shutdown() = 0;

    /// Resize the viewport
    virtual void resize(int width, int height) = 0;

    /// Begin a new frame
    virtual void begin_frame() = 0;

    /// End and present the frame
    virtual void end_frame() = 0;

    // --- Resource Management ---

    /// Create a texture from DDT data
    virtual TextureHandle create_texture(const DdtTexture& ddt) = 0;

    /// Create a texture from raw RGBA data
    virtual TextureHandle create_texture(uint32_t width, uint32_t height,
                                          const uint8_t* rgba_data) = 0;

    /// Update existing texture data (dynamic textures such as Fog of War)
    virtual void update_texture(TextureHandle handle, uint32_t width, uint32_t height,
                                const uint8_t* rgba_data) = 0;

    /// Destroy a texture
    virtual void destroy_texture(TextureHandle handle) = 0;

    /// Create a mesh from vertex/index data
    virtual MeshHandle create_mesh(const Vertex* vertices, uint32_t num_vertices,
                                    const uint32_t* indices, uint32_t num_indices) = 0;

    /// Create a terrain mesh
    virtual MeshHandle create_terrain_mesh(const TerrainVertex* vertices,
                                            uint32_t num_vertices,
                                            const uint32_t* indices,
                                            uint32_t num_indices) = 0;

    /// Destroy a mesh
    virtual void destroy_mesh(MeshHandle handle) = 0;

    // --- Rendering Commands ---

    /// Set the camera for this frame
    virtual void set_camera(const Camera& camera) = 0;

    /// Set the sun light
    virtual void set_light(const DirectionalLight& light) = 0;

    /// Draw a mesh with a transform and texture
    virtual void draw_mesh(MeshHandle mesh, const glm::mat4& transform,
                           TextureHandle texture, BlendMode blend = BlendMode::OPAQUE) = 0;

    /// Draw terrain
    virtual void draw_terrain(MeshHandle mesh, TextureHandle base_texture,
                               TextureHandle blend_texture) = 0;

    /// Draw water plane
    virtual void draw_water(const glm::vec4& water_plane,
                            TextureHandle normal_map, float time) = 0;

    /// Draw a skybox / cubemap using 6 face textures.
    /// @param faces  Array of 6 texture handles in order: [up, down, front, back, left, right]
    /// @param radius  Skybox radius in world units
    virtual void draw_skybox(const TextureHandle faces[6], float radius = 65.0f) {
        (void)faces; (void)radius;
    }

    // --- UI Rendering ---

    /// Begin UI rendering pass (2D overlay)
    virtual void begin_ui() = 0;

    /// Draw a textured quad (UI element)
    virtual void draw_ui_quad(const glm::vec2& pos, const glm::vec2& size,
                               TextureHandle texture, uint32_t color = 0xFFFFFFFF) = 0;

    /// Draw UI text (requires font texture atlas)
    virtual void draw_ui_text(const std::string& text, const glm::vec2& pos,
                               float size, uint32_t color = 0xFFFFFFFF) = 0;

    /// End UI rendering pass
    virtual void end_ui() = 0;

    // --- Debug ---

    /// Draw a debug line
    virtual void draw_debug_line(const glm::vec3& from, const glm::vec3& to,
                                  uint32_t color) = 0;

    /// Get current FPS
    [[nodiscard]] virtual float get_fps() const = 0;

    /// Get renderer name
    [[nodiscard]] virtual const char* get_name() const = 0;

    /// Set platform asset manager (AAssetManager* on Android)
    virtual void set_asset_manager(void* asset_mgr) { (void)asset_mgr; }

    /// Set font data buffer directly from memory
    virtual bool set_font_data(const uint8_t* data, size_t size) {
        (void)data; (void)size;
        return false;
    }

    /// Create a dynamic mesh whose vertex positions/normals can be updated every frame.
    /// Semantics identical to create_mesh(), but the VBO is allocated as GL_DYNAMIC_DRAW.
    /// @return mesh handle, or INVALID_MESH on failure
    virtual MeshHandle create_dynamic_mesh(const Vertex* vertices, uint32_t num_vertices,
                                           const uint32_t* indices, uint32_t num_indices) {
        // Default: fall back to static mesh (renderers that don't need morph can ignore this)
        return create_mesh(vertices, num_vertices, indices, num_indices);
    }

    /// Upload new vertex positions and normals into an existing dynamic mesh VBO.
    /// Only positions (xyz) and normals (xyz) are updated; UVs and colors remain unchanged.
    /// @param handle  A handle previously returned by create_dynamic_mesh()
    /// @param positions  Array of num_vertices position vectors
    /// @param normals    Array of num_vertices normal vectors (may be nullptr to skip)
    /// @param num_vertices  Vertex count (must match the original create call)
    virtual void update_mesh_vertices(MeshHandle handle,
                                      const glm::vec3* positions,
                                      const glm::vec3* normals,
                                      uint32_t num_vertices) {
        (void)handle; (void)positions; (void)normals; (void)num_vertices;
    }
};

/// Create an OpenGL ES 3.2 renderer
std::unique_ptr<IRenderer> create_gles_renderer();

/// Create a Vulkan renderer  
std::unique_ptr<IRenderer> create_vulkan_renderer();

} // namespace aom
