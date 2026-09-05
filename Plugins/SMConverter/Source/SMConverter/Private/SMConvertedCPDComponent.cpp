// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMConvertedCPDComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

void USMConvertedCPDComponent::OnRegister()
{
	Super::OnRegister();

	// OnRegister fires when this actor's components are (re)registered with
	// the world - that happens on normal editor level load, when entering
	// Play In Editor, and in packaged builds, so a single hook here covers
	// all three cases (BeginPlay alone would only cover the last two).
	ApplyBakedCustomPrimitiveData();
}

#if WITH_EDITOR
void USMConvertedCPDComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Reflect edits to BakedCustomPrimitiveData immediately in the editor
	// viewport, so this array is the only place that ever needs editing.
	ApplyBakedCustomPrimitiveData();
}
#endif

void USMConvertedCPDComponent::ApplyBakedCustomPrimitiveData() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	UPrimitiveComponent* Prim = Owner->FindComponentByClass<UPrimitiveComponent>();
	if (!Prim)
	{
		return;
	}

	for (int32 i = 0; i < BakedCustomPrimitiveData.Num(); ++i)
	{
		Prim->SetCustomPrimitiveDataFloat(i, BakedCustomPrimitiveData[i]);
	}
}