#include "cathub.h"
#include "file.h"
#include "hooks.h"

namespace minskill
{
void draw()
{
    if (ImGui::Begin("Minimalistic Custom Skill Menu"))
    {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded())
        {
            ImGui::Text("Menu disabled when player character is not loaded.");
            return;
        }

        ConfigReader::getSingleton()->draw();
    }
    ImGui::End();
}

bool integrateCatHub()
{
    logger::info("Looking for CatHub...");

    auto result = cathub::RequestCatHubAPI();
    if (result.index() == 0)
    {
        auto cathub_api = std::get<0>(result);
        ImGui::SetCurrentContext(cathub_api->getContext());
        cathub_api->addMenu("Minimalistic Custom Skills Menu", draw);
        logger::info("CatHub integration succeed!");
        return true;
    }
    else
    {
        logger::warn("CatHub integration failed! Mod disabled. Error: {}", std::get<1>(result));
        return false;
    }
}

void processMessage(SKSE::MessagingInterface::Message* a_msg)
{
    cathub::CatHubAPI* cathub;
    switch (a_msg->type)
    {
        case SKSE::MessagingInterface::kDataLoaded:
            logger::info("Game: data loaded");

            if (integrateCatHub())
            {
                ConfigReader::getSingleton()->readAllConfig();

                stl::write_thunk_call<UpdateHook>();
            }
            break;
        default:
            break;
    }
}
} // namespace minskill

bool installLog()
{
    auto path = logger::log_directory();
    if (!path)
        return false;

    *path /= fmt::format(FMT_STRING("{}.log"), SKSE::PluginDeclaration::GetSingleton()->GetName());
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef DBGMSG
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
#else
    log->set_level(spdlog::level::trace);
    log->flush_on(spdlog::level::trace);
#endif

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%H:%M:%S:%e][%5l] %v"s);

    return true;
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    using namespace minskill;

    installLog();

    auto* plugin  = SKSE::PluginDeclaration::GetSingleton();
    auto  version = plugin->GetVersion();
    logger::info("{} {} is loading...", plugin->GetName(), version);

    SKSE::Init(skse);
    SKSE::AllocTrampoline(14);

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener("SKSE", processMessage))
        return false;

    logger::info("{} has finished loading.", plugin->GetName());
    return true;
}