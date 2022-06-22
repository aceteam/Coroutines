# ACE Team Coroutines Plugin for Unreal Engine
If you're familiar with Unity's coroutines and ever wanted something similar to use with Unreal Engine from C++, this plugin does exactly that, but with a broader feature set.

Compared to Unity's coroutines, this system makes it much simpler to compose coroutines, so one can wait for another, run in parallel or interrupt another.

If you're unfamiliar with coroutines, they are a way to write code in a way that is similar to a function, but it doesn't have to be executed all at once. It can be suspended at certain points and continue its execution in later frames.

The nomenclature and feature set of this plugin was inspired by [**SkookumScript**](https://github.com/EpicSkookumScript/SkookumScript-Plugin) and the use we gave it during development of [**The Eternal Cylinder**](https://www.eternalcylinder.com).

## Basic Usage
As with any other C++ module, you need to add the **"ACETeam_Coroutines"** module to the list of dependencies of your module's .Build.cs file

It's recommended that you include the following headers:
- CoroutineElements.h to have access to the building blocks for your coroutines.
- CoroutinesSubsystem.h for a simple way to run your coroutine. The UCoroutinesSubsystem can execute coroutines in any circumstance. Even in the editor while it's not in play mode.

For simplicity you can add a "using namespace" declaration so you don't have to preface coroutine elements with the ACETeam_Coroutines namespace.

Many of the features of the system are explored in the following code sample:

```c++
...

#include "CoroutineElements.h"
#include "CoroutinesSubsystem.h"

using namespace ACETeam_Coroutines;

...

FCoroutineNodePtr _CoroutineTest(UWorld* World, FString TextToLog)
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

void UTestObject::RunTestCoroutine()
{
    UCoroutinesSubsystem::Get().StartCoroutine( _CoroutineTest(GetWorld(), "<test string>") );
}
```

The prefixed **_** before each Coroutine is a convention inspired by ***SkookumScript***, as are the names of some of the primitives such as **_Race** and **_Sync**.

## Full List of Coroutine Elements:
- **_Seq**: Concatenates coroutine elements in a sequence. It runs one after the other until one of them fails.
- **_Race**: Spawns an execution branch for each of its contained elements, in the declared order. Aborts any running branches when one finishes its execution (successfully or unsuccessfully).
- **_Sync**: Spawns an execution branch for each of its contained elements, in the declared order. Waits for all of them to finish their execution. If one of them failed during execution, the end result will be a failure.
- **_Wait**: Pauses execution of its branch for the specified time.
- **_WaitFrames**: Pauses execution of its branch for the specified number of frames.
- **_Loop**: Evaluates its contained element once per frame, until it fails.
- **_LoopSeq**: Shortcut for _Loop containing a _Seq
- **_Scope**: Executes the lambda contained within its first set of parentheses when the contained execution branch stops executing for any reason.
```c++
_Scope([]{ UE_LOG(LogTemp, Log, TEXT("Scope exit"); })
(
...
)
```
- **_Fork**: Spawns an independent execution branch for the contained element. This means if the original branch is aborted, it will not affect this spawned branch.
- **_Weak**: Used to indicate a lambda should not be evaluated if an associated UObject is no longer valid. Returns a failure if the lambda has a return type.

#### Any of the previous building blocks can receive an argument of 4 possible types:
1. A coroutine node (which is also the return type of any of these blocks)
2. A lambda with no return value.
3. A lambda with boolean return value. This is especially useful for terminating _Loop blocks
4. A lambda that returns a coroutine node. This allows deferring of the creation of a subcoroutine until execution reaches this lambda.

None of the lambdas should receive arguments. You can omit the () except in the case of mutable lambdas, where it's required by the compiler.

**IMPORTANT:** You should only use capture by copy in these lambdas unless you're absolutely certain the lifetime of an object captured by reference will completely overlap the lifetime of your entire coroutine's execution.

## Future work planned
A feature that **SkookumScript** had and this implementation is lacking is waiting on events, which can also pass parameters.
This allows coroutines to receive data asynchronously from non-coroutine systems, or other coroutines.
This can currently only be emulated with _Loops and shared variables, which is not as readable or performant.
