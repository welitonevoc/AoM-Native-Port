#include "data/sound_def.h"

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

bool SoundDefManager::load_soundsets_from_xmb(const XmbElement& root) {
    bool ok = false;

    for (const auto& child : root.children) {
        if (!child || child->name != "soundset") continue;

        SoundSetDef ss;
        ss.name = child->get_attribute("name");
        if (ss.name.empty()) continue;

        ss.volume = child->get_attribute_as_float("volume", 1.0f);
        ss.max_num = child->get_attribute_as_int("maxnum", 1);

        for (const auto& snd : child->children) {
            if (!snd || snd->name != "sound") continue;
            SoundEntry se;
            se.filename = snd->get_attribute("filename");
            se.volume = snd->get_attribute_as_float("volume", 1.0f);
            if (!se.filename.empty()) {
                ss.sounds.push_back(std::move(se));
            }
        }

        if (!ss.sounds.empty()) {
            const std::string key = to_lower(ss.name);
            auto it = soundset_index_.find(key);
            if (it != soundset_index_.end()) {
                soundsets_[it->second] = std::move(ss);
            } else {
                soundset_index_[key] = soundsets_.size();
                soundsets_.push_back(std::move(ss));
            }
            ok = true;
        }
    }

    return ok;
}

bool SoundDefManager::load_protounit_sounds_from_xmb(const XmbElement& root) {
    bool ok = false;

    for (const auto& child : root.children) {
        if (!child || child->name != "protounit") continue;

        ProtoUnitSoundDef psd;
        psd.unit_name = child->get_attribute("name");
        if (psd.unit_name.empty()) continue;

        for (const auto& st : child->children) {
            if (!st || st->name != "soundtype") continue;
            ProtoUnitSoundType pst;
            pst.type_name = st->get_attribute("name");

            for (const auto& inner : st->children) {
                if (!inner) continue;

                if (inner->name == "soundset") {
                    pst.sound_sets.push_back(inner->get_attribute("name"));
                }
                else if (inner->name == "damagetypelogic") {
                    for (const auto& choice : inner->children) {
                        if (!choice || choice->name != "choice") continue;
                        auto dmg_type = choice->get_attribute("name");
                        for (const auto& ss : choice->children) {
                            if (ss && ss->name == "soundset") {
                                pst.damage_type_to_set[dmg_type] = ss->get_attribute("name");
                            }
                        }
                    }
                }
                else if (inner->name == "targetlogic") {
                    for (const auto& choice : inner->children) {
                        if (!choice || choice->name != "choice") continue;
                        auto target = choice->get_attribute("name");
                        for (const auto& ss : choice->children) {
                            if (ss && ss->name == "soundset") {
                                pst.target_to_set[target] = ss->get_attribute("name");
                            }
                        }
                    }
                }
            }

            psd.sound_types.push_back(std::move(pst));
        }

        if (!psd.sound_types.empty()) {
            const std::string key = to_lower(psd.unit_name);
            auto it = protounit_index_.find(key);
            if (it != protounit_index_.end()) {
                protounit_sounds_[it->second] = std::move(psd);
            } else {
                protounit_index_[key] = protounit_sounds_.size();
                protounit_sounds_.push_back(std::move(psd));
            }
            ok = true;
        }
    }

    return ok;
}

const SoundSetDef* SoundDefManager::find_soundset(const std::string& name) const {
    auto it = soundset_index_.find(to_lower(name));
    if (it != soundset_index_.end()) return &soundsets_[it->second];
    return nullptr;
}

const ProtoUnitSoundDef* SoundDefManager::find_protounit_sound(const std::string& unit_name) const {
    auto it = protounit_index_.find(to_lower(unit_name));
    if (it != protounit_index_.end()) return &protounit_sounds_[it->second];
    return nullptr;
}

const std::string* SoundDefManager::pick_sound(const SoundSetDef& set, uint32_t seed) const {
    if (set.sounds.empty()) return nullptr;
    auto idx = seed % static_cast<uint32_t>(set.sounds.size());
    return &set.sounds[idx].filename;
}

} // namespace aom
