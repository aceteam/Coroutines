// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

class UCoroutinesWorldSubsystem;

namespace ACETeam_Coroutines
{
enum EStatus
{
	Completed = 1<<1,
	Failed = 1<<2,
	Running = 1<<3,
	Suspended = 1<<4,
	Aborted = 1<<5,
};
	
ACETEAM_COROUTINES_API const TCHAR* ToString(EStatus Status);
	
class FCoroutineNode;
constexpr ESPMode DefaultSPMode = ESPMode::NotThreadSafe;
typedef TSharedPtr<FCoroutineNode, DefaultSPMode> FCoroutineNodePtr;
typedef TSharedRef<FCoroutineNode, DefaultSPMode> FCoroutineNodeRef;

template<typename T>
constexpr bool TIsCoroutineNodeRef_V = false;

template<typename TSharedRefArg>
constexpr bool TIsCoroutineNodeRef_V<TSharedRef<TSharedRefArg, DefaultSPMode>> = TIsDerivedFrom<TSharedRefArg, FCoroutineNode>::Value;

class FCoroutineExecutor;

class ACETEAM_COROUTINES_API FCoroutineNode
{
public:
	virtual ~FCoroutineNode(){}
	virtual EStatus Start(FCoroutineExecutor* Exec) { return Running; }
	virtual EStatus Update(FCoroutineExecutor* Exec, float dt) { return Running; }
	virtual void End(FCoroutineExecutor* Exec, EStatus Status) {}
	virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) { return Running; }

#if WITH_ACETEAM_COROUTINE_DEBUGGER
private:
	friend class FGameplayDebuggerCategory_Coroutines;
	friend class FCoroutineExecutor;
	friend class ::UCoroutinesWorldSubsystem;
	virtual FString Debug_GetName() const { return FString(); }
	virtual bool Debug_IsDeferredNodeGenerator() const { return false; }
	virtual bool Debug_IsDebuggerScope() const { return false; }
	virtual int32 Debug_GetCpuTraceId() const { return 0; }
#endif
};

//WORKAROUND FOR MISSING TEMPLATES FROM UE5
#if	ENGINE_MAJOR_VERSION < 5
	template <typename T, typename DerivedType>
	struct TIsPointerOrObjectPtrToBaseOfImpl
	{
		enum { Value = false };
	};

	template <typename T, typename DerivedType>
	struct TIsPointerOrObjectPtrToBaseOfImpl<T*, DerivedType>
	{
		enum { Value = std::is_base_of_v<DerivedType, T> };
	};

	template <typename T, typename DerivedType>
	struct TIsPointerOrObjectPtrToBaseOf
	{
		enum { Value = TIsPointerOrObjectPtrToBaseOfImpl<std::remove_cv_t<T>, DerivedType>::Value };
	};

	/**
	 * Trait which determines whether or not a type is a TArray.
	 */
	template <typename T> constexpr bool TIsTArray_V = false;

	template <typename InElementType, typename InAllocatorType> constexpr bool TIsTArray_V<               TArray<InElementType, InAllocatorType>> = true;
	template <typename InElementType, typename InAllocatorType> constexpr bool TIsTArray_V<const          TArray<InElementType, InAllocatorType>> = true;
	template <typename InElementType, typename InAllocatorType> constexpr bool TIsTArray_V<      volatile TArray<InElementType, InAllocatorType>> = true;
	template <typename InElementType, typename InAllocatorType> constexpr bool TIsTArray_V<const volatile TArray<InElementType, InAllocatorType>> = true;

	/**
	 * Traits class which determines whether or not a type is a TSet.
	 */
	template <typename T> struct TIsTSet { enum { Value = false }; };

	template <typename ElementType, typename KeyFuncs, typename Allocator> struct TIsTSet<               TSet<ElementType, KeyFuncs, Allocator>> { enum { Value = true }; };
	template <typename ElementType, typename KeyFuncs, typename Allocator> struct TIsTSet<const          TSet<ElementType, KeyFuncs, Allocator>> { enum { Value = true }; };
	template <typename ElementType, typename KeyFuncs, typename Allocator> struct TIsTSet<      volatile TSet<ElementType, KeyFuncs, Allocator>> { enum { Value = true }; };
	template <typename ElementType, typename KeyFuncs, typename Allocator> struct TIsTSet<const volatile TSet<ElementType, KeyFuncs, Allocator>> { enum { Value = true }; };

	/**
	 * Traits class which determines whether or not a type is a TMap.
	 */
	template <typename T> struct TIsTMap { enum { Value = false }; };

	template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs> struct TIsTMap<               TMap<KeyType, ValueType, SetAllocator, KeyFuncs>> { enum { Value = true }; };
	template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs> struct TIsTMap<const          TMap<KeyType, ValueType, SetAllocator, KeyFuncs>> { enum { Value = true }; };
	template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs> struct TIsTMap<      volatile TMap<KeyType, ValueType, SetAllocator, KeyFuncs>> { enum { Value = true }; };
	template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs> struct TIsTMap<const volatile TMap<KeyType, ValueType, SetAllocator, KeyFuncs>> { enum { Value = true }; };
#endif
	
//Coroutine variables (shared)
template <typename T>
class TCoroVar : public TSharedRef<T>
{
};

//Creates a coroutine variable that can be referenced across coroutine blocks. Checks that the type is not a raw UObject pointer or TObjectPtr as that is not safe unless you are sure what you're doing
template <typename T, typename... InArgTypes>
TCoroVar<T> CoroVar(InArgTypes&&... Args)
{
	static_assert(!TIsPointerOrObjectPtrToBaseOf<T, UObject>::Value, "Coroutine variables should not be of type UObject* or TObjectPtr<...>, since these can turn into dangling pointers, use TWeakObjectPtr<...> instead");
	if constexpr(TIsTArray_V<T> || TIsTSet<T>::Value)
	{
		typedef typename T::ElementType U;
		static_assert(!TIsPointerOrObjectPtrToBaseOf<U, UObject>::Value, "Coroutine variables should not contain types UObject* or TObjectPtr<...>, since these can turn into dangling pointers, use TWeakObjectPtr<...> instead");
	}
	if constexpr(TIsTMap<T>::Value)
	{
		typedef typename T::KeyType K;
		typedef typename T::ValueType V;
		static_assert(!TIsPointerOrObjectPtrToBaseOf<K, UObject>::Value && !TIsPointerOrObjectPtrToBaseOf<V, UObject>::Value, "Coroutine variables should not contain types UObject* or TObjectPtr<...>, since these can turn into dangling pointers, use TWeakObjectPtr<...> instead");
	}
	auto Return = ::MakeShared<T>(::Forward<InArgTypes>(Args)...);
	return static_cast<TCoroVar<T>&>(Return);
}

//Same as CoroVar but does not check for UObject direct pointer types, use at your own risk
//Can safely be used if you only need the value for equality comparisons with another, safe UObject pointer.
template <typename T, typename... InArgTypes>
TCoroVar<T> CoroVarUnsafe(InArgTypes&&... Args)
{
	auto Return = ::MakeShared<T>(::Forward<InArgTypes>(Args)...);
	return static_cast<TCoroVar<T>&>(Return);
}

}
