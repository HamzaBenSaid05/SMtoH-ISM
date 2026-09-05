// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMConvertedCPDComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

/**
 * Called when the component is registered with the world.
 * This method is triggered during normal editor level load, when entering Play In Editor,
 * and in packaged builds. It ensures that the baked custom primitive data is applied
 * consistently across all scenarios.
 */
void USMConvertedCPDComponent::OnRegister()
{
	Super::OnRegister();

	// Apply the baked custom primitive data to the associated primitive component.
	ApplyBakedCustomPrimitiveData();
}

#if WITH_EDITOR
/**
 * Called after a property is edited in the editor.
 * This method ensures that changes to the BakedCustomPrimitiveData array are immediately
 * reflected in the editor viewport, making this array the single source of truth for editing.
 *
 * @param PropertyChangedEvent The event containing information about the changed property.
 */
void USMConvertedCPDComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Apply the baked custom primitive data to reflect changes in the editor viewport.
	ApplyBakedCustomPrimitiveData();
}
#endif

/**
 * Applies the baked custom primitive data to the associated primitive component.
 * This method retrieves the owner actor and its primitive component, then iterates
 * through the BakedCustomPrimitiveData array to set the custom primitive data floats.
 */
void USMConvertedCPDComponent::ApplyBakedCustomPrimitiveData() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return; // Exit if the owner actor is not valid.
	}

	UPrimitiveComponent* Prim = Owner->FindComponentByClass<UPrimitiveComponent>();
	if (!Prim)
	{
		return; // Exit if no primitive component is found.
	}

	// Iterate through the baked custom primitive data and apply it to the primitive component.
	for (int32 i = 0; i < BakedCustomPrimitiveData.Num(); ++i) { Prim->SetCustomPrimitiveDataFloat(i, BakedCustomPrimitiveData[i]); }
}
