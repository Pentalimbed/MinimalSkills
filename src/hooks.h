#pragma once

#include "file.h"
namespace minskill
{
struct UpdateHook
{
    inline static void thunk(RE::Main* a_this, float a2)
    {
        func(a_this, a2);
        for (const auto& skill_config : ConfigReader::getSingleton()->configs)
            if (skill_config.loaded)
            {
                auto val = std::lround(skill_config.g_show_lvl_up->value);
                if (val > 0)
                {
                    RE::DebugNotification(fmt::format("Skill \"{}\" has reached lvl. {}", skill_config.name, val).c_str());
                    RE::PlaySound("UISkillIncreaseSD");
                    skill_config.g_show_lvl_up->value = 0;
                }
            }
    }
    static inline REL::Relocation<decltype(thunk)> func;

    static constexpr auto id     = RELOCATION_ID(35551, 36544);
    static constexpr auto offset = REL::VariantOffset(0x11F, 0x160, 0);
};
} // namespace minskill