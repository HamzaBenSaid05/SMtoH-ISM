// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SMConvertedCPDComponent.generated.h"

/**
 * Helper component attached by FSMtoISMConverter to actors it creates or
 * reverts, whenever there is Custom Primitive Data that can't be relied on
 * to persist by itself.
 *
 * UPrimitiveComponent::SetCustomPrimitiveDataFloat() only sets run-time
 * render data and is explicitly documented by Epic as NOT serialized:
 * https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/Rendering/Material/SetCustomPrimitiveDataFloat
 *
 * A value set that way looks correct immediately in the editor viewport,
 * but is lost the moment the owning level is closed and reopened, entered
 * in Play In Editor, or packaged - none of those go through the code path
 * that originally set it.
 *
 * This component stores the values as a normal (serialized) UPROPERTY and
 * is the SINGLE source of truth for them from this point on: it reapplies
 * them to the owning primitive whenever its components are (re)registered
 * - which covers editor level load, PIE, and packaged builds with one
 * mechanism - and again whenever BakedCustomPrimitiveData itself is edited
 * in the Details panel, so the edit is reflected immediately without
 * anything silently overwriting it later.
 *
 * After the SM/ISM converter attaches this component, always edit CPD
 * values HERE (BakedCustomPrimitiveData), not via any other path - this is
 * the only copy that is guaranteed to persist and be picked up again.
 */
UCLASS(ClassGroup = (SMConverter), meta = (BlueprintSpawnableComponent))
class SMCONVERTER_API USMConvertedCPDComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** CPD values to (re)apply, at their original DataIndex position. Edit this, not the runtime CPD directly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM Converter",
		meta = (DisplayName = "Custom Primitive Data (edit here, this is the source of truth)"))
	TArray<float> BakedCustomPrimitiveData;

	/** Re-applies BakedCustomPrimitiveData to the owning primitive component right now. */
	UFUNCTION(BlueprintCallable, Category = "SM Converter")
	void ApplyBakedCustomPrimitiveData() const;

protected:
	virtual void OnRegister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};