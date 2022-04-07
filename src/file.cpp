#include "file.h"
#include "utils.h"

#include <regex>
#include <sstream>
#include "toml++/toml.h"

#include "ImNodes/ImNodesEz.h"

namespace minskill
{
const fs::path config_dir    = "data/NetScriptFramework/Plugins";
const auto     config_prefix = "CustomSkill."sv;
const auto     config_suffix = ".config.txt"sv;

const ImVec2 scale         = {70, 400};
auto         none_color    = IM_COL32(150, 150, 150, 255); // not obtained perk color
auto         partial_color = IM_COL32(255, 102, 25, 255);  // partly obtained perk color
auto         full_color    = IM_COL32(0, 230, 153, 255);   // fully obtained perk color
auto         error_color   = IM_COL32(255, 20, 20, 255);   // error text color

struct TempPerk
{
    std::vector<uint16_t> links;

    bool         enabled = false;
    RE::BGSPerk* perk;
    int          gridx = 0, gridy = 0;
    float        x = 0, y = 0;

    std::string plugin;
    RE::FormID  id;
};

void SkillConfig::read(const fs::path& path)
{
    this->path = path;
    if (!fs::is_regular_file(path))
    {
        logger::error("File {} does not exist. This shouldn't happen. Please report to author!", path.string());
        return;
    }

    toml::table tbl;
    try
    {
        tbl = toml::parse_file(path.string());
    }
    catch (toml::parse_error err)
    {
        std::ostringstream strstrm;
        strstrm << err;
        logger::error("Failed to parse file {}.\n\tError: {}", path.string(), strstrm.str());
        return;
    }

    name = tbl["Name"].as_string()->get();
    desc = tbl["Description"].as_string()->get();

    g_skill_lvl = getForm<RE::TESGlobal>(tbl["LevelFile"].value_or<std::string>(""), (RE::FormID)tbl["LevelId"].value_or<int64_t>(0));
    if (!g_skill_lvl)
    {
        logger::error("Cannot find value of LevelFile or LevelId");
        return;
    }
    g_lvl_ratio = getForm<RE::TESGlobal>(tbl["RatioFile"].value_or<std::string>(""), (RE::FormID)tbl["RatioId"].value_or<int64_t>(0));
    if (!g_lvl_ratio)
    {
        logger::error("Cannot find value of RatioFile or RatioId");
        return;
    }
    g_show_lvl_up = getForm<RE::TESGlobal>(tbl["ShowLevelupFile"].value_or<std::string>(""), (RE::FormID)tbl["ShowLevelupId"].value_or<int64_t>(0));
    if (!g_show_lvl_up)
    {
        logger::error("Cannot find value of ShowLevelupFile or ShowLevelupId");
        return;
    }
    g_perk_pts   = getForm<RE::TESGlobal>(tbl["PerkPointsFile"].value_or<std::string>(""), (RE::FormID)tbl["PerkPointsId"].value_or<int64_t>(0));
    g_legend_cts = getForm<RE::TESGlobal>(tbl["LegendaryFile"].value_or<std::string>(""), (RE::FormID)tbl["LegendaryId"].value_or<int64_t>(0));

    // Read perks
    std::map<uint16_t, TempPerk> temp_perks;
    for (const auto& [key, val] : tbl)
    {
        std::string key_str(key.str());
        std::smatch m;
        std::regex  node_regex("^Node([0-9]+)");
        if (std::regex_match(key_str, m, node_regex))
        {
            uint16_t num = std::stoi(m[1].str());
            if (num == 0) continue;
            auto node_tbl = *val.as_table();

            TempPerk temp;

            temp.enabled = node_tbl["Enable"].value_or<bool>(false);
            if (!temp.enabled) continue;

            temp.plugin = node_tbl["PerkFile"].value_or<std::string>("");
            temp.id     = node_tbl["PerkId"].value_or<int64_t>(0);
            temp.x      = node_tbl["X"].value_or<double>(0);
            temp.y      = node_tbl["Y"].value_or<double>(0);
            temp.gridx  = node_tbl["GridX"].value_or<int64_t>(0);
            temp.gridy  = node_tbl["GridY"].value_or<int64_t>(0);
            parseStrList(temp.links, node_tbl["Links"].value_or<std::string>(""));

            temp_perks[num] = temp;
        }
    }

    // Find perk form & skill req
    std::list<uint16_t> disabled_nodes;
    for (auto& [num, perk] : temp_perks)
    {
        if (perk.enabled)
        {
            perk.perk = getForm<RE::BGSPerk>(perk.plugin, perk.id);
            if (!perk.perk)
            {
                logger::warn("Cannot find perk {:x} in {}. Perk disabled.", perk.id, perk.plugin.data());
                perk.enabled = false;
            }
        }
        if (!perk.enabled)
            disabled_nodes.push_back(num);
    }
    // Propagate disabled perks
    for (auto disabled_iter = disabled_nodes.begin(); disabled_iter != disabled_nodes.end(); disabled_iter++)
        for (auto linked_num : temp_perks[*disabled_iter].links)
            if (temp_perks[linked_num].enabled)
            {
                temp_perks[linked_num].enabled = false;
                disabled_nodes.push_back(linked_num);
            }

    // Fill in actual perks
    perks.clear();
    for (const auto& [num, temp_perk] : temp_perks)
    {
        if (temp_perk.enabled)
        {
            auto& curr_perk = perks[num] = Perk();

            curr_perk.perk  = temp_perk.perk;
            curr_perk.pos.y = temp_perk.gridx * scale.x + temp_perk.x * scale.x;
            curr_perk.pos.x = temp_perk.gridy * scale.y + temp_perk.y * scale.y;
            for (const auto& link : temp_perk.links)
                if (temp_perks[link].enabled)
                    curr_perk.links.push_back(link);
            auto newest_perk = curr_perk.perk;
            while (newest_perk)
            {
                newest_perk = newest_perk->nextPerk;
                curr_perk.vers++;
            }
        }
    }

    loaded = true;
}

void SkillConfig::draw()
{
    if (!loaded)
    {
        ImGui::Text("Failed to load config {}.\n\tPlease check log at [My Games/Skyrim Special Edition/SKSE/MinimalisticSkillMenu.log].", path.c_str());
        return;
    }

    auto player = RE::PlayerCharacter::GetSingleton();

    ImGui::Text(fmt::format("{}  lvl. {}", name, std::lround(g_skill_lvl->value)).c_str());
    ImGui::ProgressBar(g_lvl_ratio->value, ImVec2(-1.0f, 0.0f));
    if (g_legend_cts)
    {
        long                    legend_cts        = std::lround(g_legend_cts->value);
        static bool             failed_legend     = false;
        static std::string_view cannot_legend_str = "Not enough skill level!";
        if (std::lround(g_skill_lvl->value) >= 100)
            if (ImGui::Button("Set Legendary"))
                setLegendary();
        ImGui::SameLine();
        ImGui::Text(fmt::format("Legnedary Count: {}", legend_cts).c_str());
    }
    ImGui::Text(fmt::format("Perk Points: {}", g_perk_pts ? (int8_t)g_perk_pts->value : RE::PlayerCharacter::GetSingleton()->perkCount).c_str());

    // Skill tree
    if (ImGui::Begin(fmt::format("Perk Tree ({})", name).c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImNodes::Ez::BeginCanvas();

        for (auto& [num, perk_info] : perks)
        {
            auto    newest_perk = perk_info.perk;
            uint8_t curr_ver    = 0;
            while (player->HasPerk(newest_perk) && newest_perk->nextPerk)
            {
                curr_ver++;
                newest_perk = newest_perk->nextPerk;
            }
            curr_ver += player->HasPerk(newest_perk);

            ImNodes::Ez::SlotInfo input  = {"req", 1};
            ImNodes::Ez::SlotInfo output = {"", 1};

            auto color  = curr_ver ? (curr_ver == perk_info.vers ? full_color : partial_color) : none_color;
            bool popped = false;
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            if (ImNodes::Ez::BeginNode(&perk_info, newest_perk->GetName(), &perk_info.pos, &perk_info.selected))
            {
                ImGui::PopStyleColor();
                popped = true;

                ImNodes::Ez::InputSlots(&input, 1);
                ImGui::Text(fmt::format("({}/{})", curr_ver, perk_info.vers).c_str());
                ImNodes::Ez::OutputSlots(&output, 1);
                ImNodes::Ez::EndNode();
            }
            if (!popped)
                ImGui::PopStyleColor();
        }

        for (auto& [num, perk_info] : perks)
            for (auto link_num : perk_info.links)
                ImNodes::Connection(&perks[link_num], "req", &perk_info, "");

        ImNodes::Ez::EndCanvas();

        ImGui::End();
    }

    ImGui::Separator();
    // Perk Info
    bool has_selected = false;
    for (auto& [num, perk_info] : perks)
    {
        if (perk_info.selected)
        {
            has_selected = true;
            drawPerkInfo(perk_info);
            break;
        }
    }

    if (!has_selected)
        ImGui::Text("Select a perk to see more info.");
}

void SkillConfig::setLegendary()
{
    auto player = RE::PlayerCharacter::GetSingleton();

    g_skill_lvl->value = 0;

    for (auto& [num, perk_info] : perks)
        for (auto newest_perk = perk_info.perk; newest_perk && player->HasPerk(newest_perk); newest_perk = newest_perk->nextPerk)
        {
            if (g_perk_pts)
                g_perk_pts->value += 1;
            else
                player->perkCount++;
            player->RemovePerk(newest_perk);
        }

    g_legend_cts->value += 1;
}

void SkillConfig::drawPerkInfo(Perk& perk_info)
{
    auto player = RE::PlayerCharacter::GetSingleton();

    auto newest_perk = perk_info.perk;
    while (player->HasPerk(newest_perk) && newest_perk->nextPerk)
        newest_perk = newest_perk->nextPerk;

    ImGui::Text("Perk: %s", newest_perk->GetName());

    long skill_req  = 0;
    long legend_req = 0;

    for (auto conditem = newest_perk->perkConditions.head; conditem; conditem = conditem->next)
    {
        if ((conditem->data.functionData.function == RE::FUNCTION_DATA::FunctionID::kGetGlobalValue) &&
            (conditem->data.flags.opCode == RE::CONDITION_ITEM_DATA::OpCode::kGreaterThanOrEqualTo))
        {
            auto param = std::bit_cast<ConditionParam>(conditem->data.functionData.params[0]).form;
            if ((uintptr_t)param == (uintptr_t)g_skill_lvl)
                skill_req = std::lround(conditem->data.comparisonValue.f);
            if ((uintptr_t)param == (uintptr_t)g_legend_cts)
                legend_req = std::lround(conditem->data.comparisonValue.f);
        }
    }
    ImGui::Text("Skill Needed: %ld", skill_req);
    ImGui::SameLine();
    ImGui::Text("Legendary Needed: %ld", legend_req);

    RE::BSString perk_desc = "";
    newest_perk->GetDescription(perk_desc, newest_perk);
    ImGui::Text("Description: %s", perk_desc.c_str());

    static bool             get_perk_failed = false;
    static std::string_view failed_reason   = "";
    if (ImGui::Button("Get Perk"))
    {
        if (!newest_perk->perkConditions.IsTrue(player, nullptr))
        {
            get_perk_failed = true;
            failed_reason   = "Perk condition not met!";
        }
        else
        {
            if (g_perk_pts)
            {
                if (std::lround(g_perk_pts->value) > 0)
                {
                    player->AddPerk(newest_perk);
                    g_perk_pts->value -= 1;
                    get_perk_failed = false;
                    RE::PlaySound("UISkillsPerkSelect2D");
                }
                else
                {
                    get_perk_failed = true;
                    failed_reason   = "Not enough perk points!";
                }
            }
            else
            {
                if (player->perkCount > 0)
                {
                    player->AddPerk(newest_perk);
                    player->perkCount -= 1;
                    get_perk_failed = false;
                    RE::PlaySound("UISkillsPerkSelect2D");
                }
                else
                {
                    get_perk_failed = true;
                    failed_reason   = "Not enough perk points!";
                }
            }
        }
    }
    if (get_perk_failed)
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, error_color);
        ImGui::Text(failed_reason.data());
        ImGui::PopStyleColor();
    }
}

void ConfigReader::readAllConfig()
{
    logger::info("Reading configs!");
    if (fs::exists(config_dir))
    {
        for (const auto& entry : fs::directory_iterator{config_dir})
        {
            if (entry.is_regular_file())
            {
                auto filename = entry.path().filename().string();
                if (filename.starts_with(config_prefix) && filename.ends_with(config_suffix))
                {
                    logger::info("Reading {}", entry.path().string());
                    SkillConfig config;
                    config.read(entry.path());
                    configs.push_back(config);
                }
            }
        }
    }
    logger::info("{} configs read.", configs.size());
}

void ConfigReader::draw()
{
    static ImNodes::Ez::Context* context = ImNodes::Ez::CreateContext();

    if (configs.size() > 0)
    {
        if (ImGui::BeginTabBar("Skills", ImGuiTabBarFlags_None))
        {
            for (auto& config : configs)
            {
                if (ImGui::BeginTabItem(config.name.c_str()))
                {
                    config.draw();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    else
    {
        ImGui::Text("No skill config loaded.");
    }
}

} // namespace minskill