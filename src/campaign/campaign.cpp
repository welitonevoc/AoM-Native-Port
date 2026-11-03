#include "campaign/campaign.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace aom {

namespace {

std::string extract_attribute(const std::string& tag, const std::string& attr_name) {
    std::string needle = attr_name + " = \"";
    size_t pos = tag.find(needle);
    if (pos == std::string::npos) {
        needle = attr_name + "=\"";
        pos = tag.find(needle);
    }
    if (pos == std::string::npos) {
        needle = attr_name + " = '";
        pos = tag.find(needle);
    }
    if (pos == std::string::npos) {
        needle = attr_name + "='";
        pos = tag.find(needle);
    }
    if (pos == std::string::npos) {
        return "";
    }
    pos += needle.size();
    char quote = tag[pos - 1];
    size_t end_pos = tag.find(quote, pos);
    if (end_pos == std::string::npos) {
        return "";
    }
    return tag.substr(pos, end_pos - pos);
}

std::string extract_tag_content(const std::string& xml, const std::string& tag_name, size_t start_pos, size_t end_pos) {
    std::string open_tag = "<" + tag_name;
    size_t open_pos = xml.find(open_tag, start_pos);
    if (open_pos == std::string::npos || open_pos >= end_pos) {
        return "";
    }
    size_t tag_close = xml.find('>', open_pos);
    if (tag_close == std::string::npos || tag_close >= end_pos) {
        return "";
    }
    std::string close_tag = "</" + tag_name + ">";
    size_t content_end = xml.find(close_tag, tag_close);
    if (content_end == std::string::npos || content_end >= end_pos) {
        return "";
    }
    std::string content = xml.substr(tag_close + 1, content_end - (tag_close + 1));
    // Trim
    size_t first = content.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = content.find_last_not_of(" \t\r\n");
    return content.substr(first, last - first + 1);
}

} // anonymous namespace

bool CampaignManager::load_campaign(const std::string& filepath) {
    std::ifstream in(filepath);
    if (!in.is_open()) {
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    return parse_campaign_xml(content);
}

bool CampaignManager::load_campaign_from_memory(const uint8_t* data, size_t size) {
    if (!data || size == 0) return false;
    std::string content(reinterpret_cast<const char*>(data), size);
    return parse_campaign_xml(content);
}

bool CampaignManager::parse_campaign_xml(const std::string& xml) {
    campaign_ = Campaign{};
    loaded_ = false;
    current_scenario_idx_ = 0;

    // Find <campaign ...>
    size_t camp_pos = xml.find("<campaign");
    if (camp_pos == std::string::npos) {
        return false;
    }
    size_t camp_tag_end = xml.find('>', camp_pos);
    if (camp_tag_end == std::string::npos) {
        return false;
    }
    std::string camp_tag = xml.substr(camp_pos, camp_tag_end - camp_pos + 1);
    campaign_.name = extract_attribute(camp_tag, "name");
    std::string ver_str = extract_attribute(camp_tag, "version");
    if (!ver_str.empty()) {
        campaign_.version = std::stoi(ver_str);
    }
    campaign_.next_campaign = extract_attribute(camp_tag, "nextcampaign");

    // Parse <BackgroundSet> tags
    size_t bg_pos = 0;
    while ((bg_pos = xml.find("<BackgroundSet", bg_pos)) != std::string::npos) {
        size_t bg_tag_end = xml.find('>', bg_pos);
        if (bg_tag_end == std::string::npos) break;
        std::string bg_tag = xml.substr(bg_pos, bg_tag_end - bg_pos + 1);
        std::string bg_name = extract_attribute(bg_tag, "name");

        size_t bg_end = xml.find("</BackgroundSet>", bg_tag_end);
        if (bg_end == std::string::npos) bg_end = xml.size();

        std::vector<std::string> images;
        size_t img_pos = bg_tag_end;
        while ((img_pos = xml.find("<Image", img_pos)) != std::string::npos && img_pos < bg_end) {
            size_t img_open_end = xml.find('>', img_pos);
            size_t img_close = xml.find("</Image>", img_open_end);
            if (img_open_end != std::string::npos && img_close != std::string::npos && img_close < bg_end) {
                std::string img_path = xml.substr(img_open_end + 1, img_close - img_open_end - 1);
                size_t f = img_path.find_first_not_of(" \t\r\n");
                size_t l = img_path.find_last_not_of(" \t\r\n");
                if (f != std::string::npos && l != std::string::npos) {
                    images.push_back(img_path.substr(f, l - f + 1));
                }
                img_pos = img_close + 8;
            } else {
                break;
            }
        }
        campaign_.background_sets[bg_name] = std::move(images);
        bg_pos = bg_end + 16;
    }

    // Parse <Scenario ...> tags
    size_t sc_pos = 0;
    while ((sc_pos = xml.find("<Scenario", sc_pos)) != std::string::npos) {
        size_t sc_tag_end = xml.find('>', sc_pos);
        if (sc_tag_end == std::string::npos) break;

        // Check if self-closing <Scenario ... />
        bool self_closing = (xml[sc_tag_end - 1] == '/');
        std::string sc_tag = xml.substr(sc_pos, sc_tag_end - sc_pos + 1);

        CampaignScenario scenario;
        scenario.display_name = extract_attribute(sc_tag, "dispName");
        scenario.filename = extract_attribute(sc_tag, "filename");
        scenario.background_set = extract_attribute(sc_tag, "BackgroundSet");
        scenario.unlisted = (sc_tag.find("unlisted") != std::string::npos);

        if (!self_closing) {
            size_t sc_end = xml.find("</Scenario>", sc_tag_end);
            if (sc_end != std::string::npos) {
                // Parse child overlays
                size_t ov_pos = sc_tag_end;
                while ((ov_pos = xml.find("<Overlay", ov_pos)) != std::string::npos && ov_pos < sc_end) {
                    size_t ov_tag_end = xml.find('>', ov_pos);
                    size_t ov_close = xml.find("</Overlay>", ov_tag_end);
                    if (ov_tag_end != std::string::npos && ov_close != std::string::npos && ov_close < sc_end) {
                        std::string ov_tag = xml.substr(ov_pos, ov_tag_end - ov_pos + 1);
                        std::string tex = xml.substr(ov_tag_end + 1, ov_close - ov_tag_end - 1);
                        size_t f = tex.find_first_not_of(" \t\r\n");
                        size_t l = tex.find_last_not_of(" \t\r\n");

                        ScenarioOverlay overlay;
                        if (f != std::string::npos && l != std::string::npos) {
                            overlay.texture = tex.substr(f, l - f + 1);
                        }

                        std::string off_str = extract_attribute(ov_tag, "offset");
                        if (!off_str.empty()) {
                            std::istringstream oss(off_str);
                            oss >> overlay.offset_x >> overlay.offset_y;
                        }

                        std::string sz_str = extract_attribute(ov_tag, "size");
                        if (!sz_str.empty()) {
                            std::istringstream ssz(sz_str);
                            ssz >> overlay.size_w >> overlay.size_h;
                        }

                        scenario.overlays.push_back(std::move(overlay));
                        ov_pos = ov_close + 10;
                    } else {
                        break;
                    }
                }
                sc_pos = sc_end + 11;
            } else {
                sc_pos = sc_tag_end + 1;
            }
        } else {
            sc_pos = sc_tag_end + 1;
        }

        campaign_.scenarios.push_back(std::move(scenario));
    }

    loaded_ = !campaign_.scenarios.empty();
    return loaded_;
}

const CampaignScenario* CampaignManager::get_scenario(size_t index) const {
    if (index < campaign_.scenarios.size()) {
        return &campaign_.scenarios[index];
    }
    return nullptr;
}

const CampaignScenario* CampaignManager::find_scenario_by_filename(const std::string& filename) const {
    for (const auto& sc : campaign_.scenarios) {
        if (sc.filename == filename) return &sc;
        // Case-insensitive or partial match
        if (sc.filename.find(filename) != std::string::npos || filename.find(sc.filename) != std::string::npos) {
            return &sc;
        }
    }
    return nullptr;
}

const CampaignScenario* CampaignManager::find_scenario_by_name(const std::string& name) const {
    for (const auto& sc : campaign_.scenarios) {
        if (sc.display_name.find(name) != std::string::npos) return &sc;
    }
    return nullptr;
}

const CampaignScenario* CampaignManager::current_scenario() const {
    return get_scenario(current_scenario_idx_);
}

bool CampaignManager::advance_scenario() {
    if (current_scenario_idx_ + 1 < campaign_.scenarios.size()) {
        ++current_scenario_idx_;
        return true;
    }
    return false;
}

bool CampaignManager::set_scenario_index(size_t index) {
    if (index < campaign_.scenarios.size()) {
        current_scenario_idx_ = index;
        return true;
    }
    return false;
}

} // namespace aom
