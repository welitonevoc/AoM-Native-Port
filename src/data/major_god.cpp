#include "data/major_god.h"

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

int parse_attr_int(const XmbElement& elem, const std::string& attr_name, int def = 0) {
    auto v = elem.get_attribute(attr_name);
    if (v.empty()) return def;
    try { return std::stoi(v); } catch (...) { return def; }
}

} // anonymous namespace

bool GodRegistry::load_majorgods_from_xmb(const XmbElement& root) {
    bool ok = false;

    for (const auto& child : root.children) {
        if (!child || child->name != "god") continue;

        MajorGodData g;
        g.name_id = parse_attr_int(*child, "nameid");
        g.tech_name = child->get_attribute("techname");
        g.lg_portrait = child->get_attribute("lgportrait");
        g.sm_portrait = child->get_attribute("tnyportrait");
        g.md_portrait = child->get_attribute("smportrait");
        g.lg2_portrait = child->get_attribute("mdportrait");
        g.position = parse_attr_int(*child, "position");

        // Detecta civilização pelo techname
        auto lname = to_lower(g.tech_name);
        if (lname.find("zeus") != std::string::npos || lname.find("poseidon") != std::string::npos ||
            lname.find("hades") != std::string::npos) {
            g.civilization = "greek";
        } else if (lname.find("ra") != std::string::npos || lname.find("isis") != std::string::npos ||
                   lname.find("set") != std::string::npos) {
            g.civilization = "egypt";
        } else if (lname.find("odin") != std::string::npos || lname.find("thor") != std::string::npos ||
                   lname.find("loki") != std::string::npos) {
            g.civilization = "norse";
        } else if (lname.find("ouranos") != std::string::npos || lname.find("cronos") != std::string::npos ||
                   lname.find("gaia") != std::string::npos) {
            g.civilization = "atlantis";
        }

        for (const auto& mg : child->children) {
            if (!mg || mg->name != "minorgod") continue;
            MinorGodData mgd;
            mgd.tech_name = mg->get_attribute("techname");
            mgd.age = parse_attr_int(*mg, "age");
            mgd.position = parse_attr_int(*mg, "position");
            // A civilização é inferida do major god pai
            mgd.civilization = g.civilization;
            g.minor_gods.push_back(std::move(mgd));
        }

        if (!g.tech_name.empty()) {
            const std::string key = to_lower(g.tech_name);
            auto it = major_index_.find(key);
            if (it != major_index_.end()) {
                major_gods_[it->second] = std::move(g);
            } else {
                major_index_[key] = major_gods_.size();
                major_gods_.push_back(std::move(g));
            }
            ok = true;
        }
    }

    return ok;
}

bool GodRegistry::load_minorgods_from_xmb(const XmbElement& root) {
    bool ok = false;

    for (const auto& child : root.children) {
        if (!child || child->name != "god") continue;

        MinorGodData m;
        m.dbid = parse_attr_int(*child, "dbid");
        m.name_id = parse_attr_int(*child, "nameid");
        m.tech_name = child->get_attribute("techname");
        m.portrait = child->get_attribute("portrait");
        m.age = parse_attr_int(*child, "age");
        m.position = parse_attr_int(*child, "position");

        // Detecta civilização
        auto lname = to_lower(m.tech_name);
        if (lname.find("athena") != std::string::npos || lname.find("ares") != std::string::npos ||
            lname.find("hermes") != std::string::npos || lname.find("apollo") != std::string::npos ||
            lname.find("aphrodite") != std::string::npos || lname.find("artemis") != std::string::npos ||
            lname.find("hephaestus") != std::string::npos || lname.find("hera") != std::string::npos ||
            lname.find("dionysos") != std::string::npos || lname.find("hestia") != std::string::npos) {
            m.civilization = "greek";
        } else if (lname.find("anubis") != std::string::npos || lname.find("bast") != std::string::npos ||
                   lname.find("ptah") != std::string::npos || lname.find("hathor") != std::string::npos ||
                   lname.find("nephthys") != std::string::npos || lname.find("sekhmet") != std::string::npos ||
                   lname.find("osiris") != std::string::npos || lname.find("horus") != std::string::npos ||
                   lname.find("thoth") != std::string::npos || lname.find("ra") != std::string::npos ||
                   lname.find("isis") != std::string::npos || lname.find("set") != std::string::npos) {
            m.civilization = "egypt";
        } else if (lname.find("odin") != std::string::npos || lname.find("thor") != std::string::npos ||
                   lname.find("loki") != std::string::npos || lname.find("forseti") != std::string::npos ||
                   lname.find("heimdall") != std::string::npos || lname.find("freyja") != std::string::npos ||
                   lname.find("skadi") != std::string::npos || lname.find("bragi") != std::string::npos ||
                   lname.find("njord") != std::string::npos || lname.find("hel") != std::string::npos ||
                   lname.find("baldr") != std::string::npos || lname.find("tyr") != std::string::npos ||
                   lname.find("freyr") != std::string::npos) {
            m.civilization = "norse";
        } else if (lname.find("okeanus") != std::string::npos || lname.find("prometheus") != std::string::npos ||
                   lname.find("leto") != std::string::npos || lname.find("hyperion") != std::string::npos ||
                   lname.find("theia") != std::string::npos || lname.find("rheia") != std::string::npos ||
                   lname.find("helios") != std::string::npos || lname.find("hekate") != std::string::npos ||
                   lname.find("atlas") != std::string::npos || lname.find("gaia") != std::string::npos ||
                   lname.find("cronos") != std::string::npos || lname.find("ouranos") != std::string::npos) {
            m.civilization = "atlantis";
        }

        if (!m.tech_name.empty()) {
            const std::string key = to_lower(m.tech_name);
            auto it = minor_index_.find(key);
            if (it != minor_index_.end()) {
                minor_gods_[it->second] = std::move(m);
            } else {
                minor_index_[key] = minor_gods_.size();
                minor_gods_.push_back(std::move(m));
            }
            ok = true;
        }
    }

    return ok;
}

const MajorGodData* GodRegistry::find_major(const std::string& tech_name) const {
    auto it = major_index_.find(to_lower(tech_name));
    if (it != major_index_.end()) return &major_gods_[it->second];
    return nullptr;
}

const MinorGodData* GodRegistry::find_minor(const std::string& tech_name) const {
    auto it = minor_index_.find(to_lower(tech_name));
    if (it != minor_index_.end()) return &minor_gods_[it->second];
    return nullptr;
}

std::vector<const MinorGodData*> GodRegistry::find_minors_by_civ(const std::string& civ) const {
    std::vector<const MinorGodData*> result;
    auto lciv = to_lower(civ);
    for (const auto& m : minor_gods_) {
        if (to_lower(m.civilization) == lciv) result.push_back(&m);
    }
    return result;
}

} // namespace aom
