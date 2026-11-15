#pragma once

/// @file button_set.h
/// @brief Gerenciador de conjuntos de botões (buttonsets) do Age of Mythology.
///        Decodifica arquivos buttonsets.xmb/buttonsets.xml e mapeia os estados
///        de textura (normal, depressed, highlight, disabled) e coordenadas UV.

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <glm/glm.hpp>
#include "assets/xmb_parser.h"
#include "assets/bar_archive.h"

namespace aom {

/// Definição de texturas e propriedades de um conjunto de botões
struct ButtonStateDef {
    std::string name;
    std::string normal_tex;
    std::string depressed_tex;
    std::string highlight_tex;
    std::string disabled_tex;
    glm::vec4 texture_coords{0.0f, 0.0f, 1.0f, 1.0f};
    bool has_texture_coords = false;
    bool has_bevel = false;
};

/// Gerencia e armazena os buttonsets extraídos de buttonsets.xmb
class ButtonSetManager {
public:
    ButtonSetManager() = default;
    ~ButtonSetManager() = default;

    /// Carrega os buttonsets a partir dos dados brutos de um arquivo XMB
    bool load_from_xmb_data(const std::vector<uint8_t>& data);

    /// Carrega os buttonsets a partir de um arquivo dentro do BAR (data.bar)
    bool load_from_bar(BarArchive& bar, const std::string& filename = "buttonsets.xmb");

    /// Retorna a definição de um conjunto de botões pelo nome (ex: "buttonLong", "checkBtn")
    [[nodiscard]] const ButtonStateDef* get_set(const std::string& name) const;

    /// Retorna se possui o conjunto
    [[nodiscard]] bool has_set(const std::string& name) const;

    /// Retorna o número de conjuntos carregados
    [[nodiscard]] size_t count() const { return sets_.size(); }

    /// Limpa todos os conjuntos carregados
    void clear() { sets_.clear(); }

    /// Retorna todos os conjuntos (para iteração/debug)
    [[nodiscard]] const std::unordered_map<std::string, ButtonStateDef>& all_sets() const {
        return sets_;
    }

private:
    std::unordered_map<std::string, ButtonStateDef> sets_;
};

} // namespace aom
