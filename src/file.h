#pragma once

#include <filesystem>

namespace minskill
{
namespace fs = std::filesystem;

struct Perk
{
    std::vector<uint16_t> links;
    RE::BGSPerk*          perk;
    uint8_t               vers = 0;
    ImVec2                pos;
    bool                  selected = false;
};


struct SkillConfig
{
    bool     loaded = false;
    fs::path path;

    std::string name = "Failed";
    std::string desc;
    // global vars
    RE::TESGlobal* g_skill_lvl;
    RE::TESGlobal* g_lvl_ratio;
    RE::TESGlobal* g_show_lvl_up;
    RE::TESGlobal* g_perk_pts;
    RE::TESGlobal* g_legend_cts;

    std::map<uint16_t, Perk> perks;

    void read(const fs::path& path);
    void draw();

    void drawPerkInfo(Perk& perk);
    void setLegendary();
};

class ConfigReader
{
public:
    static ConfigReader* getSingleton()
    {
        static ConfigReader reader;
        return std::addressof(reader);
    }

    void readAllConfig();
    void draw();

    std::vector<SkillConfig> configs;
};

} // namespace minskill