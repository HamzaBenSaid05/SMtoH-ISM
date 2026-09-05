// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * SMConverterStyle is a utility class responsible for managing the visual style of the SMConverter plugin.
 * It provides methods to initialize and shut down the style set, as well as retrieve style-related information.
 */
class SMCONVERTER_API SMConverterStyle
{
public:
	/**
	 * Initializes the SMConverter style set.
	 * This method sets up the style set, including its content root and any associated resources.
	 */
	static void Initialize();

	/**
	 * Shuts down the SMConverter style set.
	 * This method unregisters the style set and releases any associated resources.
	 */
	static void Shutdown();

	/**
	 * Retrieves the current SMConverter style set.
	 * @return A reference to the current ISlateStyle instance.
	 */
	static const ISlateStyle& Get();

	/**
	 * Retrieves the name of the SMConverter style set.
	 * @return The name of the style set as an FName.
	 */
	static FName GetStyleSetName();

private:
	/** A shared pointer to the FSlateStyleSet instance used for the SMConverter style. */
	static TSharedPtr<FSlateStyleSet> StyleSet;
};
