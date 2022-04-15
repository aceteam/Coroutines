// Copyright ACE Team Software S.A. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FACETeam_CoroutinesModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
