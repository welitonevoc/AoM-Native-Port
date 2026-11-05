#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace aom {

class BarArchive;

/// Definicao das 6 faces de um Cube Map (.cub) do Age of Mythology
struct CubeMapDef {
    std::string up;
    std::string down;
    std::string front;
    std::string back;
    std::string left;
    std::string right;

    bool is_valid() const {
        return !up.empty() || !down.empty() || !front.empty() ||
               !back.empty() || !left.empty() || !right.empty();
    }
};

/// Faz parse do conteudo de texto de um arquivo .cub
CubeMapDef parse_cub_file(const std::string& cub_content);

/// Carrega a definicao do cubemap a partir de um arquivo em disco ou de um BarArchive
CubeMapDef load_cub_definition(const std::string& filename, BarArchive* archive = nullptr);

} // namespace aom
