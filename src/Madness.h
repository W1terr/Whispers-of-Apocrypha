#pragma once

// The madness of Apocrypha's ink: time spent in the black water (the vanilla Dragonborn damage effect is active)
// builds up exposure; exposure brings whispers, then the green nightmare (a vanilla image space modifier turned
// green), then Hermaeus Mora's own eyes and faces in the dark around you, and at last Hermaeus Mora himself.
namespace Madness
{
	void InstallHooks();
	void Init();          // data loaded: forms
	void Reset();         // a game is about to be loaded, or a new one starts
	void OnGameLoaded();  // removes the eyes a save may have kept
}
