#include <iostream>
#include "assets/bar_archive.h"
#include "assets/xmb_parser.h"

void print_xmb(const aom::XmbElement* elem, int depth = 0) {
    if (!elem) return;
    std::string indent(depth * 2, ' ');
    std::cout << indent << "<" << elem->name;
    for (const auto& attr : elem->attributes) {
        std::cout << " " << attr.name << "=\"" << attr.value << "\"";
    }
    if (!elem->text.empty()) {
        std::cout << ">" << elem->text;
    } else if (elem->children.empty()) {
        std::cout << "/>";
        std::cout << "\n";
        return;
    } else {
        std::cout << ">";
    }
    std::cout << "\n";
    for (const auto& child : elem->children) {
        print_xmb(child.get(), depth + 1);
    }
    if (!elem->text.empty() || !elem->children.empty()) {
        std::cout << indent << "</" << elem->name << ">\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <data_root>\n";
        return 1;
    }
    std::string root = argv[1];
    std::string data_base = root + "/data/data.bar";
    std::string data_exp = root + "/data/data2.bar";
    
    aom::BarArchive merged;
    if (!merged.open_pair(data_base, data_exp)) {
        std::cerr << "Failed to open BARs\n";
        return 1;
    }
    
    // Try to find and extract uicampaignsetup.xmb
    std::vector<uint8_t> data = merged.extract("uicampaignsetup.xmb");
    if (!data.empty()) {
        std::cout << "=== uicampaignsetup.xmb ===\n";
        aom::XmbParser parser;
        auto root_elem = aom::XmbParser::parse(data.data(), data.size());
        if (root_elem) {
            print_xmb(root_elem.get());
        } else {
            std::cout << "Failed to parse\n";
        }
    } else {
        std::cout << "uicampaignsetup.xmb not found\n";
    }
    
    // Also try uicampaign.xmb
    data = merged.extract("uicampaign.xmb");
    if (!data.empty()) {
        std::cout << "\n=== uicampaign.xmb ===\n";
        auto root_elem = aom::XmbParser::parse(data.data(), data.size());
        if (root_elem) {
            print_xmb(root_elem.get());
        } else {
            std::cout << "Failed to parse\n";
        }
    } else {
        std::cout << "uicampaign.xmb not found\n";
    }
    return 0;
}