// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineElements.h"

namespace ACETeam_Coroutines
{
	/**
	 * Used to generate children for a composite node dynamically.
	 * The composite node you pass in can already have children, and the generated ones will be appended after the
	 * original children.
	 * 
	 * Usage example:
	 * _GenerateChildren(_Sync(), [&](auto AddChild)
	 *	{
	 *		for(int i = 0; i < NumChildren; ++i)
	 *		{
	 *			AddChild(_MyChild(i));
	 *		}
	 *	})
	 */
	template <typename TLambda>
	FCoroutineCompositeRef _GenerateChildren(FCoroutineCompositeRef&& Composite, TLambda const& ChildGenerator)
	{
		ChildGenerator([&](FCoroutineNodeRef&& Child)
		{
			AddCoroutineChild(Composite, Child);
		});
		check(Composite->GetNumChildren() > 0);
		return Composite;
	}
}