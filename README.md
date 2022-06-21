# ACE Team Coroutines Plugin for Unreal Engine

This plugin was developed for the purpose of having an easy way to write routines that handle control flow spread across multiple frames in a robust and interruptible way. It's useful for scheduling async operations, reactive AI (e.g. behavior trees), animated visualizations, and other operations which you might want to spread over several frames.

It's significantly simpler to write non-trivial logic across several frames using this system than it is using FTimerManager and other similar mechanisms.

Full disclosure: It hasn't been used in production of any of our games yet, but it is based on a lot of experience gathered on several similar systems I ([@daniel-amthauer](https://github.com/daniel-amthauer)) have worked on along the years.


## Background
Our experience with [**SkookumScript**](https://github.com/EpicSkookumScript/SkookumScript-Plugin) during development of [**The Eternal Cylinder**](https://www.eternalcylinder.com) proved that it was very useful to have the expressive power to easily spread out control flow over several frames, but the dwindling support for the plugin after Epic's acquisition made it too difficult to maintain across Unreal Engine versions.

Also the addition of live coding made it viable to have fairly quick iteration times from C++.

The combination of these two factors led us to seek an implementation that captured most of the capabilities of **SkookumScript** without requiring a separate scripting language, and a very complex hard to maintain codebase.

## Basic Usage

As in any other C++ module, you need to add the "ACETeam_Coroutines" module to the list of dependencies of your module's .Build.cs file

It's recommended that you include the following headers:
- CoroutineElements.h to have access to the building blocks for your coroutines.
- CoroutinesSubsystem.h for a simple way to run your coroutine. The UCoroutinesSubsystem can execute coroutines in any circumstance. Even in the editor while it's not in play mode.

For simplicity you can add a using namespace declaration so you don't have to preface coroutine elements with the ACETeam_Coroutines namespace.

Many of the features of the system are explored in the following code sample:

```c++
...

#include "CoroutineElements.h"
#include "CoroutinesSubsystem.h"

using namespace ACETeam_Coroutines;

...

FCoroutineNodePtr _CoroutineTest(UWorld* World, FString TextToLog)
{
    auto SharedValue = MakeShared<int>(0);
    //_Seq concatenates coroutine elements in a sequence. It runs one after the other until one of them fails
    //If one of them finishes during a frame, the next in the sequence will be evaluated in the same frame
    return _Seq(
        //_Race evaluates each of its contained elements once per frame, in the declared order, until one finishes its execution (successfully or unsuccessfully)
        _Race(
            //a _Loop will run its contained element once per frame, until it fails or is aborted
            _Loop(
                //_Weak creates an element that will only evaluate its lambda if the passed in object is valid on evaluation
                _Weak(World, [] { DrawDebugPoint(World, FVector::ZeroVector, 10, FColor::White); })
            ),
            //_LoopSeq is a shortcut for a _Loop containing a _Seq
            _LoopSeq(
                //_Wait will pause this execution branch for the specified time
                _Wait(1.0f),
                [=] {
                    UE_LOG(LogTemp, Log, TEXT("This text will print once per second"));
                    UE_LOG(LogTemp, Log, TEXT("This parameter was captured by value %s"), *TextToLog);
                },
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

void UTestObject::RunTestCoroutine()
{
    UCoroutinesSubsystem::Get().StartCoroutine( _CoroutineTest(GetWorld(), "<test string>") );
}
```

The prefixed **_** before each Coroutine is a convention inspired by ***SkookumScript***, as are the names of some of the primitives such as **_Race** and **_Sync**.

## Full List of Coroutine Elements:
- **_Seq**: Concatenates coroutine elements in a sequence. It runs one after the other until one of them fails.
- **_Race**: Evaluates each of its contained elements once per frame, in the declared order, until one finishes its execution (successfully or unsuccessfully). 
- **_Sync**: Evaluates each of its contained elements once per frame, in the declared order, until all of them have finished their execution. If one of them failed during execution, the end result will be a failure
- **_Wait**: Pauses execution of its branch for the specified time.
- **_WaitFrames**: Pauses execution of its branch for the specified number of frames.
- **_Loop**: Evaluates its contained element once per frame, until it fails.
- **_LoopSeq**: Shortcut for _Loop containing a _Seq
- **_Scope**: Executes its first argument when the contained execution branch stops executing for any reason
```c++
_Scope([]{ UE_LOG(LogTemp, Log, TEXT("Scope exit"); })
(
...
)
```
- **_Fork**: Spawns a separate execution branch for the contained element. If the original branch is aborted, it will not affect this spawned branch.
- **_Weak**: Used to indicate a lambda should not be evaluated if an associated UObject is no longer valid