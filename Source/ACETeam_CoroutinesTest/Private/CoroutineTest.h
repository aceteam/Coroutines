// Copyright ACE Team Software S.A. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CoroutineTest.generated.h"

UCLASS()
class ACoroutineTest : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACoroutineTest();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
};
