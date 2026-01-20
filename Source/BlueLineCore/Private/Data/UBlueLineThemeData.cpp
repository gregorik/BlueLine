// Copyright YourTeamName. All Rights Reserved.

#include "Data/UBlueLineThemeData.h"

FLinearColor UBlueLineThemeData::GetColorForTag(const FGameplayTag& Tag) const
{
	// fail-safe for empty/invalid tags
	if (!Tag.IsValid())
	{
		return DefaultTagColor;
	}

	// Pointer to the best matching rule found so far
	const FBlueLineTagStyle* BestMatch = nullptr;
	int32 BestMatchSpecificity = -1;

	for (const FBlueLineTagStyle& Rule : TagStyles)
	{
		// Skip invalid rules
		if (!Rule.Tag.IsValid())
		{
			continue;
		}

		// 1. Check for Exact Match (Highest Priority)
		// If we find an exact match, we can stop immediately.
		if (Rule.Tag == Tag)
		{
			return Rule.Color;
		}

		// 2. Check for Inheritance (Parent Match)
		// logic: InputTag.MatchesTag(RuleTag) returns true if Input is a Child of Rule
		if (Rule.bApplyToChildren && Tag.MatchesTag(Rule.Tag))
		{
			// We found a valid parent. Now we must determine if this parent is "closer" 
			// than any previous parent we found.
			//
			// Example:
			// Input: "Status.Debuff.Burn"
			// Rule A: "Status" (Generic)
			// Rule B: "Status.Debuff" (Specific)
			// Both match. But Rule B is better.
			//
			// We use the length of the tag string as a proxy for "Specificity/Depth".
			// "Status.Debuff" is longer than "Status", so it wins.
			
			const int32 Specificity = Rule.Tag.ToString().Len();

			if (Specificity > BestMatchSpecificity)
			{
				BestMatchSpecificity = Specificity;
				BestMatch = &Rule;
			}
		}
	}

	// If we found a hierarchical match, return it
	if (BestMatch)
	{
		return BestMatch->Color;
	}

	// No specific rules found, return global default
	return DefaultTagColor;
}