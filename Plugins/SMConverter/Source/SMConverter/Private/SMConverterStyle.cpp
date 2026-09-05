// Fill out your copyright notice in the Description page of Project Settings.

#include "SMConverterStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"
#include "Brushes/SlateImageBrush.h"
#include "Misc/Paths.h"

// Static member variable to hold the style set instance
TSharedPtr<FSlateStyleSet> SMConverterStyle::StyleSet;

/**
 * Initializes the SMConverter style set.
 * - Creates a new style set if it doesn't already exist.
 * - Sets the content root to the plugin's "Resources" directory.
 * - Registers a vector image brush for the style.
 * - Registers the style set with the Slate Style Registry.
 */
void SMConverterStyle::Initialize()
{
	if (StyleSet.IsValid())
	{
		return; // Exit if the style set is already initialized
	}

	// Create a new style set with the name "SMConverterStyle"
	StyleSet = MakeShared<FSlateStyleSet>("SMConverterStyle");

	// Define the path to the plugin's "Resources" directory
	const FString ResourcesPath =
		IPluginManager::Get().FindPlugin(TEXT("SMConverter"))
		                     ->GetBaseDir() / TEXT("Resources");

	// Set the content root for the style set
	StyleSet->SetContentRoot(ResourcesPath);

	// Add a vector image brush to the style set
	StyleSet->Set(
	              "SMConverter.SMConverter",
	              new FSlateVectorImageBrush(
	                                         StyleSet->RootToContentDir(TEXT("SMConverter"), TEXT(".svg")),
	                                         FVector2D(40.0f, 40.0f)
	                                        )
	             );

	// Register the style set with the Slate Style Registry
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

/**
 * Shuts down the SMConverter style set.
 * - Unregisters the style set from the Slate Style Registry.
 * - Resets the style set instance.
 */
void SMConverterStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		// Unregister the style set
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		// Reset the style set instance
		StyleSet.Reset();
	}
}

/**
 * Retrieves the current SMConverter style set.
 * @return A reference to the current style set.
 */
const ISlateStyle& SMConverterStyle::Get() { return *StyleSet; }

/**
 * Retrieves the name of the SMConverter style set.
 * @return The name of the style set as an FName.
 */
FName SMConverterStyle::GetStyleSetName() { return "SMConverterStyle"; }
