#include "data/unit_transform.h"

#include <algorithm>
#include <cctype>

namespace aom {

namespace {

std::string to_lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

} // anonymous namespace

bool UnitTransformManager::load_from_xmb(const XmbElement& root) {
    bool ok = false;

    for (const auto& child : root.children) {
        if (!child || child->name != "transform") {
            continue;
        }

        UnitTransform t;
        t.from = child->get_attribute("from");  // não existe como atributo no XML de referência, é um child
        t.to = child->get_attribute("to");

        // No XML de referência, <from> e <to> são elementos filhos com texto, não atributos
        if (t.from.empty()) {
            if (auto* from_el = child->find_child("from"); from_el) {
                t.from = from_el->text;
            }
        }
        if (t.to.empty()) {
            if (auto* to_el = child->find_child("to"); to_el) {
                t.to = to_el->text;
            }
        }

        if (!t.from.empty() && !t.to.empty()) {
            transforms_.push_back(std::move(t));
            const std::string key = to_lower(transforms_.back().from);
            from_index_[key].push_back(transforms_.size() - 1);
            ok = true;
        }
    }

    return ok;
}

std::vector<UnitTransform> UnitTransformManager::find_from(const std::string& from) const {
    std::vector<UnitTransform> result;
    auto it = from_index_.find(to_lower(from));
    if (it != from_index_.end()) {
        for (size_t idx : it->second) {
            result.push_back(transforms_[idx]);
        }
    }
    return result;
}

} // namespace aom
