#pragma once

#include "API.h"

// Built-in support for "Stress and Fear - A Dynamic Sanity System", on when Stress and Fear.esp is loaded: the dark of
// Apocrypha brings stress (growing with the madness), Hermaeus Mora's eyes bring the mod's fear reaction and his
// appearance is a trauma.
namespace StressAndFear
{
	void Init();  // data loaded: finds the mod

	// the madness's messages (API.h), handed over as they are sent
	void Handle(WhispersOfApocrypha::API::Message a_message, const void* a_data, std::uint32_t a_size);
}
