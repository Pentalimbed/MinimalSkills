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

#ifdef BUILD_SE
    static inline uint64_t id     = 35551; // 5AF3D0
    static inline size_t   offset = 0x11F;
#else
    static inline uint64_t id     = 36544; // 5D29F0
    static inline size_t   offset = 0x160;
#endif
};
} // namespace minskill