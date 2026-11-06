#pragma once

/// @file unit_transform.h
/// @brief Transformações de unidades extraídas do data.bar (unittransform.xmb).
///        Mapeia "from" → "to" para saber quando uma unidade se transforma em outra.

#include <string>
#include <vector>
#include <unordered_map>

namespace aom {

/// Uma transformação: unidade "from" se transforma em unidade "to"
struct UnitTransform {
    std::string from;
    std::string to;
};

/// Gerenciador de transformações de unidades carregado do unittransform.xmb
class UnitTransformManager {
public:
    UnitTransformManager() = default;
    ~UnitTransformManager() = default;

    /// Carrega definições de um arquivo unittransform.xmb (via XMB parseado)
    bool load_from_xmb(const XmbElement& root);

    /// Busca todas as transformações para uma unidade de origem (case-insensitive)
    [[nodiscard]] std::vector<UnitTransform> find_from(const std::string& from) const;

    [[nodiscard]] size_t count() const { return transforms_.size(); }
    [[nodiscard]] const std::vector<UnitTransform>& all() const { return transforms_; }

private:
    std::vector<UnitTransform> transforms_;
    std::unordered_map<std::string, std::vector<size_t>> from_index_;
};

} // namespace aom
