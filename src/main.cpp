#include "cathub.h"
#include "file.h"
#include "hooks.h"

namespace minskill
{
void draw()
{
    auto player = RE::PlayerCharacter::GetSingleton();
    if (!player || !player->Is3DLoaded())
    {
        ImGui::Text("Menu disabled when player character is not loaded.");
        return;
    }

    ConfigReader::getSingleton()->draw();
}

void processMessage(SKSE::MessagingInterface::Message* a_msg)
{
    cathub::CatHubAPI* cathub;
    switch (a_msg->type)
    {
        case SKSE::MessagingInterface::kDataLoaded:
            logger::debug("Data loaded");
            cathub = cathub::RequestCatHubAPI();
            if (!cathub)
            {
                logger::error("Failed to acquire CatHub API! Disabled!");
                return;
            }
            ImGui::SetCurrentContext(cathub->getContext());

            ConfigReader::getSingleton()->readAllConfig();
            cathub->addMenu("Minimalistic Custom Skills Menu", draw);
            stl::write_thunk_call<UpdateHook>();
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

    *path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifdef NDEBUG
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
#else
    log->set_level(spdlog::level::trace);
    log->flush_on(spdlog::level::trace);
#endif

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%H:%M:%S:%e] %v"s);

    return true;
}

extern "C"
{
    DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
    {
        a_info->infoVersion = SKSE::PluginInfo::kVersion;
        a_info->name        = Version::PROJECT.data();
        a_info->version     = Version::VERSION[0];

        if (a_skse->IsEditor())
        {
            logger::critical("Loaded in editor, marking as incompatible"sv);
            return false;
        }

        const auto ver = a_skse->RuntimeVersion();
        if (ver < SKSE::RUNTIME_1_5_39)
        {
            logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
            return false;
        }

        return true;
    }

#ifndef BUILD_SE
    DLLEXPORT constinit auto SKSEPlugin_Version = []() {
        SKSE::PluginVersionData v;

        v.PluginVersion(Version::VERSION);
        v.PluginName(Version::PROJECT);

        v.UsesAddressLibrary(true);
        v.CompatibleVersions({SKSE::RUNTIME_LATEST});

        return v;
    }();
#endif

    DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
    {
        installLog();

        logger::info("loaded plugin");

        SKSE::Init(a_skse);
        SKSE::AllocTrampoline(14);

        using namespace minskill;

        auto messaging = SKSE::GetMessagingInterface();
        if (!messaging->RegisterListener("SKSE", processMessage))
        {
            return false;
        }

        return true;
    }
}