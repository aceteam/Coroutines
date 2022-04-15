// Copyright ACE Team Software S.A. All Rights Reserved.

#include "ACETeam_CoroutinesModule.h"

void FACETeam_CoroutinesModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FACETeam_CoroutinesModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}


IMPLEMENT_MODULE(FACETeam_CoroutinesModule, ACETeam_Coroutines)