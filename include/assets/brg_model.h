#pragma once

#include "renderer/renderer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace aom {

class BarArchive;

/// Extracted 3D mesh from a BRG file
struct BrgMesh {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::string texture_name;
    uint32_t material_id = 0;
};

/// Complete 3D model with one or more meshes and materials
struct BrgModel {
    std::string name;
    std::vector<BrgMesh> meshes;
    std::vector<std::string> textures;
    std::string pending_texture_name; // MTRL texture name seen before any MESI
    uint32_t total_vertices = 0;
    uint32_t total_faces = 0;
};

/// Parse a BRG binary buffer directly
std::unique_ptr<BrgModel> parse_brg(const uint8_t* data, size_t size, const std::string& name = "");

/// Load a BRG model from an open BAR archive
std::unique_ptr<BrgModel> load_brg_model_data(BarArchive* archive, const std::string& name);

/// Load a BRG model from an archive and upload its primary mesh to the renderer.
/// The function also returns the texture handle that should be used for the model
/// (the diffuse texture that has the same base name with a `.ddt` extension).
/// If the texture cannot be found, `out_material` will be set to `INVALID_TEXTURE`.
MeshHandle load_brg_model(BarArchive* archive,
                         const std::string& name,
                         IRenderer* renderer);

/// Create a procedural 3D Greek Pyramid/Obelisk mesh for testing and fallback
std::unique_ptr<BrgModel> create_procedural_brg();

} // namespace aom
