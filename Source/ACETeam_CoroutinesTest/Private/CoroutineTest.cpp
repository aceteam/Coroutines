// Copyright ACE Team Software S.A. All Rights Reserved.

#include "CoroutineTest.h"

#include "CoroutineElements.h"
#include "CoroutinesSubsystem.h"
#include "DrawDebugHelpers.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, ACETeam_CoroutinesTest)

// Sets default values
ACoroutineTest::ACoroutineTest()
{
	PrimaryActorTick.bCanEverTick = false;
}

PRAGMA_DISABLE_OPTIMIZATION
using namespace ACETeam_Coroutines;
FCoroutineNodeRef _CoroutineTest(UWorld* World, FString TextToLog)
{
    //Shared ptr values can be used to share data between different execution branches,
    // or different steps in your coroutine
    //This way they're guaranteed to share the same lifetime as the code that's using them
    auto SharedValue = MakeShared<int>(0);
    //_Seq concatenates coroutine elements in a sequence. It runs one after the other until
    // one of them fails
    //If one of them finishes during a frame, the next in the sequence will be evaluated
    // in the same frame
    return _Seq(
        //_Race evaluates each of its contained elements once per frame, in the declared order, until
        // one finishes its execution (successfully or unsuccessfully)
        _Race(
            //a _Loop will run its contained element once per frame, until it fails or is aborted
            _Loop(
                //_Weak creates an element that will only evaluate its lambda if the passed in
                // object is valid on evaluation
                _Weak(World, [=] { DrawDebugPoint(World, FVector::ZeroVector, 10, FColor::White); })
            ),
            //_LoopSeq is a shortcut for a _Loop containing a _Seq
            _LoopSeq(
                //_Wait will pause this execution branch for the specified time
                _Wait(1.0f),
                [=] {
                    UE_LOG(LogTemp, Log, TEXT("This text will print once per second"));
                    UE_LOG(LogTemp, Log, TEXT("This parameter was captured by value %s"), *TextToLog);
                },
                //mutable lambdas can be used to have local variables that share their lifetime
                [Counter = 0] () mutable {
                    //When this returns false, the containing loop will finish
                    return ++Counter <= 10;
                },
                [=] {
                    //If one execution branch wants to pass data to a different
                    //branch, you can use shared variables
                    *SharedValue += FMath::RandHelper(10);
                }
            )
        ),
        [=] { 
            UE_LOG(LogTemp, Log, TEXT("This log will appear after the race above finishes"));
            UE_LOG(LogTemp, Log, TEXT("The final value stored in SharedValue is  %d"), *SharedValue); 
        }
    );
}

void ACoroutineTest::BeginPlay()
{
	Super::BeginPlay();

    UCoroutinesSubsystem::Get().StartCoroutine(_CoroutineTest(GetWorld(), TEXT("test string")));
}
