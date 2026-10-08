#include "Madness.h"

namespace
{
	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			Madness::Init();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Madness::Reset();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			Madness::OnGameLoaded();
			break;
		default:
			break;
		}
	}
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	spdlog::set_level(spdlog::level::info);
	spdlog::flush_on(spdlog::level::info);
	logs::info("Whispers of Apocrypha loading");

	Madness::InstallHooks();
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
