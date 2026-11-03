#pragma once

/// @file campaign.h
/// @brief Parser and runtime manager for Age of Mythology campaign files (.CPN / .CPX)

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace aom {

/// Represents an overlay marker icon on the campaign map
struct ScenarioOverlay {
    std::string texture;
    int offset_x = 0;
    int offset_y = 0;
    int size_w = 32;
    int size_h = 32;
};

/// Represents a single playable or cinematic scenario in a campaign
struct CampaignScenario {
    std::string display_name;
    std::string filename;       ///< Path to .scn or cinematic scenario
    std::string background_set; ///< Background artwork set identifier
    bool unlisted = false;      ///< True if cutscene/unlisted in map menu
    std::vector<ScenarioOverlay> overlays;
};

/// Represents a complete campaign structure
struct Campaign {
    std::string name;
    int version = 1;
    std::string next_campaign;
    std::unordered_map<std::string, std::vector<std::string>> background_sets;
    std::vector<CampaignScenario> scenarios;
};

/// Manager for loading and progressing through Age of Mythology campaigns
class CampaignManager {
public:
    CampaignManager() = default;
    ~CampaignManager() = default;

    /// Load and parse a .cpn or .cpx file from disk
    bool load_campaign(const std::string& filepath);

    /// Load and parse campaign XML from memory buffer
    bool load_campaign_from_memory(const uint8_t* data, size_t size);

    /// Parse raw campaign XML content
    bool parse_campaign_xml(const std::string& xml_content);

    /// Get currently loaded campaign
    [[nodiscard]] const Campaign* current_campaign() const {
        return loaded_ ? &campaign_ : nullptr;
    }

    /// Total number of scenarios in campaign
    [[nodiscard]] size_t scenario_count() const {
        return campaign_.scenarios.size();
    }

    /// Get scenario by zero-based index
    [[nodiscard]] const CampaignScenario* get_scenario(size_t index) const;

    /// Find scenario by filename (e.g. "spc\aomsp04")
    [[nodiscard]] const CampaignScenario* find_scenario_by_filename(const std::string& filename) const;

    /// Find scenario by display title (e.g. "Omens")
    [[nodiscard]] const CampaignScenario* find_scenario_by_name(const std::string& name) const;

    /// Get active scenario currently being played
    [[nodiscard]] const CampaignScenario* current_scenario() const;

    /// Get active scenario index
    [[nodiscard]] size_t current_index() const { return current_scenario_idx_; }

    /// Advance to the next scenario in campaign
    bool advance_scenario();

    /// Jump directly to a scenario by index
    bool set_scenario_index(size_t index);

    /// Reset campaign progress to beginning
    void reset_progress() { current_scenario_idx_ = 0; }

private:
    Campaign campaign_;
    bool loaded_ = false;
    size_t current_scenario_idx_ = 0;
};

} // namespace aom
