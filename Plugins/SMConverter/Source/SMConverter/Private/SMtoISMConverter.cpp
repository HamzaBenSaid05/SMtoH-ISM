// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMtoISMConverter.h"

#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "SMConvertedCPDComponent.h"

// ---------------------------------------------------------------------------
// Internal Snapshot
// ---------------------------------------------------------------------------
/**
 * Structure to store a snapshot of a Static Mesh Component's properties.
 */
struct FSMSnapshot
{
	FTransform WorldTransform;             /**< Transform of the component in the world. */
	UStaticMesh* Mesh = nullptr;           /**< Pointer to the Static Mesh. */
	TArray<UMaterialInterface*> Materials; /**< Array of materials applied to the component. */

	bool bCastShadow = true;                                        /**< Whether the component casts shadows. */
	bool bCastDynamicShadow = true;                                 /**< Whether the component casts dynamic shadows. */
	bool bCastStaticShadow = true;                                  /**< Whether the component casts static shadows. */
	bool bCastContactShadow = false;                                /**< Whether the component casts contact shadows. */
	bool bSelfShadowOnly = false;                                   /**< Whether the component casts shadows only on itself. */
	bool bReceivesDecals = true;                                    /**< Whether the component receives decals. */
	bool bRenderCustomDepth = false;                                /**< Whether the component renders in the custom depth pass. */
	int32 CustomDepthStencil = 0;                                   /**< Custom depth stencil value. */
	float BoundsScale = 1.f;                                        /**< Scale factor for the component's bounds. */
	int32 ForcedLOD = 0;                                            /**< Forced Level of Detail (LOD). */
	int32 MinLOD = 0;                                               /**< Minimum Level of Detail (LOD). */
	float MaxDrawDistance = 0.f;                                    /**< Maximum draw distance for the component. */
	int32 OverrideLightmapRes = 64;                                 /**< Resolution for the overridden lightmap. */
	bool bOverrideLightmapRes = false;                              /**< Whether to override the lightmap resolution. */
	EComponentMobility::Type Mobility = EComponentMobility::Static; /**< Mobility type of the component. */
	TArray<float> CustomDataFloats;                                 /**< Array of custom data floats for the component. */
	/*
	 * CPD indices read through Per Instance Custom Data.
	 *
	 * These values are written with SetCustomData().
	 */
	TArray<int32> PerInstanceDataIndices;

	/*
	 * CPD indices that cannot safely be represented per instance.
	 *
	 * These values are written as component-level CPD and are also
	 * part of the ISM/HISM grouping key.
	 */
	TArray<int32> ComponentDataIndices;
};

// ---------------------------------------------------------------------------
/**
 * Takes a snapshot of the properties of a given Static Mesh Component.
 *
 * @param C The Static Mesh Component to snapshot.
 * @return A snapshot of the component's properties.
 */
static FSMSnapshot SnapshotActor(UStaticMeshComponent* C)
{
	FSMSnapshot S;
	S.WorldTransform = C->GetComponentTransform();
	S.Mesh = C->GetStaticMesh();

	const int32 N = C->GetNumMaterials();

	S.Materials.Reserve(N);

	for (int32 i = 0; i < N; ++i)
		S.Materials.Add(C->GetMaterial(i));

	S.bCastShadow = C->CastShadow;
	S.bCastDynamicShadow = C->bCastDynamicShadow;
	S.bCastStaticShadow = C->bCastStaticShadow;
	S.bCastContactShadow = C->bCastContactShadow;
	S.bSelfShadowOnly = C->bSelfShadowOnly;
	S.bReceivesDecals = C->bReceivesDecals;
	S.bRenderCustomDepth = C->bRenderCustomDepth;
	S.CustomDepthStencil = C->CustomDepthStencilValue;
	S.BoundsScale = C->BoundsScale;
	S.ForcedLOD = C->ForcedLodModel;
	S.MinLOD = C->MinLOD;
	S.MaxDrawDistance = C->LDMaxDrawDistance;
	S.OverrideLightmapRes = C->OverriddenLightMapRes;
	S.bOverrideLightmapRes = C->bOverrideLightMapRes;
	S.Mobility = C->Mobility;
	/*
	* Read original CPD.
	*/
	const FCustomPrimitiveData& CPD =
		C->GetCustomPrimitiveData();

	const int32 NumCPD =
		CPD.Data.Num();

	S.CustomDataFloats.SetNum(NumCPD);

	for (int32 i = 0; i < NumCPD; ++i)
	{
		S.CustomDataFloats[i] =
			CPD.Data[i];
	}

	/*
	 * Determine how every CPD index must be represented
	 * after conversion.
	 */
	FSMtoISMConverter::DetermineCPDDataModes(
	                                         C,
	                                         NumCPD,
	                                         S.PerInstanceDataIndices,
	                                         S.ComponentDataIndices
	                                        );
	return S;
}

// ---------------------------------------------------------------------------
/**
 * Applies properties from a snapshot to an Instanced Static Mesh Component.
 *
 * @param C The Instanced Static Mesh Component to apply properties to.
 * @param Src The snapshot containing the properties to apply.
 * @param Cfg The settings for the conversion process.
 */
static void ApplyProperties(
	UInstancedStaticMeshComponent* C,
	const FSMSnapshot& Src,
	const FSMtoISMSettings& Cfg)
{
	C->SetStaticMesh(Src.Mesh);

	for (int32 i = 0; i < Src.Materials.Num(); ++i)
		C->SetMaterial(i, Src.Materials[i]);

	C->CastShadow =
		Cfg.bReadFromSource ? Src.bCastShadow : Cfg.bCastShadow;

	C->bCastDynamicShadow =
		Cfg.bReadFromSource
			? Src.bCastDynamicShadow
			: Cfg.bCastDynamicShadow;

	C->bCastStaticShadow =
		Cfg.bReadFromSource
			? Src.bCastStaticShadow
			: Cfg.bCastStaticShadow;

	C->bCastContactShadow =
		Cfg.bReadFromSource
			? Src.bCastContactShadow
			: Cfg.bCastContactShadow;

	C->bSelfShadowOnly =
		Cfg.bReadFromSource
			? Src.bSelfShadowOnly
			: Cfg.bSelfShadowOnly;

	C->bReceivesDecals =
		Cfg.bReadFromSource
			? Src.bReceivesDecals
			: Cfg.bReceivesDecals;

	C->bRenderCustomDepth =
		Cfg.bReadFromSource
			? Src.bRenderCustomDepth
			: Cfg.bRenderCustomDepth;

	C->CustomDepthStencilValue =
		Cfg.bReadFromSource
			? Src.CustomDepthStencil
			: Cfg.CustomDepthStencil;

	C->BoundsScale =
		Cfg.bReadFromSource
			? Src.BoundsScale
			: Cfg.BoundsScale;

	C->ForcedLodModel =
		Cfg.bReadFromSource
			? Src.ForcedLOD
			: Cfg.ForcedLOD;

	C->MinLOD =
		Cfg.bReadFromSource
			? Src.MinLOD
			: Cfg.MinLOD;

	C->LDMaxDrawDistance =
		Cfg.bReadFromSource
			? Src.MaxDrawDistance
			: Cfg.MaxDrawDistance;

	C->bOverrideLightMapRes =
		Cfg.bOverrideLightmapRes;

	C->OverriddenLightMapRes =
		Cfg.OverrideLightmapRes;

	C->SetMobility(Src.Mobility);
}

// ---------------------------------------------------------------------------
/**
 * Computes the hash value for a Hierarchical Instanced Static Mesh (HISM) group key.
 *
 * @param K The HISM group key to hash.
 * @return The computed hash value.
 */
FORCEINLINE uint32 GetTypeHash(const FHISMGroupKey& K)
{
	uint32 Hash = ::GetTypeHash(K.Mesh);
	Hash = HashCombine(Hash, ::GetTypeHash(K.Level));
	Hash = HashCombine(Hash, ::GetTypeHash(K.CDONum));

	/*
		 * Material hash.
		 */
	uint32 MatHash = 0;

	for (UMaterialInterface* Mat : K.Materials)
	{
		MatHash +=
			::GetTypeHash(Mat);
	}

	Hash =
		HashCombine(
		            Hash,
		            MatHash
		           );

	/*
	 * Component-level CPD values.
	 */
	for (const float Value : K.ComponentLevelValues)
	{
		Hash =
			HashCombine(
			            Hash,
			            ::GetTypeHash(Value)
			           );
	}
	return Hash;
}

// ============================================================================
// Convert from StaticMeshActor to ISM/HISM
// ============================================================================
/**
 * Converts selected Static Mesh Actors to Instanced Static Mesh (ISM) or Hierarchical Instanced Static Mesh (HISM) components.
 *
 * @param Cfg The settings for the conversion process.
 * @return A result structure containing information about the conversion.
 */
FSMtoISMResult FSMtoISMConverter::Convert(const FSMtoISMSettings& Cfg)
{
	FSMtoISMResult Result;

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Result.Log.Add(TEXT("[ERR] No editor world."));
		return Result;
	}

	UEditorActorSubsystem* Sub = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	if (!Sub)
	{
		Result.Log.Add(TEXT("[ERR] EditorActorSubsystem not available."));
		return Result;
	}
	TArray<AActor*> Selected = Sub->GetSelectedLevelActors();

	// --- 1. Snapshot ---
	TArray<UStaticMeshComponent*> Sources;
	TArray<FSMSnapshot> Snapshots;

	for (AActor* Actor : Selected)
	{
		if (!Actor) continue;

		TArray<UStaticMeshComponent*> Components;
		Actor->GetComponents<UStaticMeshComponent>(Components);

		for (UStaticMeshComponent* Comp : Components)
		{
			if (!Comp || !Comp->GetStaticMesh()) continue;
			// Skip ISM/HISM components to avoid reprocessing
			if (Comp->IsA<UInstancedStaticMeshComponent>()) continue;
			Sources.Add(Comp);
			Snapshots.Add(SnapshotActor(Comp));
			UE_LOG(LogTemp, Warning, TEXT("[SNAPSHOT] %s CPD count=%d"), *Actor->GetActorLabel(), Snapshots.Last().CustomDataFloats.Num());
		}
	}

	TSet<int32> RequiredCPDIndices;

	for (const FSMSnapshot& Snapshot : Snapshots)
	{
		for (int32 Index = 0;
		     Index < Snapshot.CustomDataFloats.Num();
		     ++Index) { RequiredCPDIndices.Add(Index); }
	}

	TArray<FString> MaterialWarnings;

	const bool bMaterialsValid =
		ValidateSelectedMaterials(
		                          Selected,
		                          RequiredCPDIndices,
		                          MaterialWarnings
		                         );

	if (!bMaterialsValid)
	{
		UE_LOG(
		       LogTemp,
		       Warning,
		       TEXT(
			       "[SMConverter] Material validation completed "
			       "with %d warning(s). Conversion will continue."
		       ),
		       MaterialWarnings.Num()
		      );

		for (const FString& Warning : MaterialWarnings)
		{
			Result.Log.Add(
			               FString::Printf(
			                               TEXT("[WARN] %s"),
			                               *Warning
			                              )
			              );
		}
	}

	if (Sources.IsEmpty())
	{
		Result.Log.Add(TEXT("[WARN] No valid StaticMeshComponents in selection."));
		return Result;
	}

	Result.Log.Add(FString::Printf(
	                               TEXT("[INFO] %d mesh components found, mode=%s"),
	                               Sources.Num(),
	                               Cfg.bUseHISM ? TEXT("HISM") : TEXT("ISM")));

	const FScopedTransaction Tx(NSLOCTEXT("SMtoISM", "Convert", "SM to ISM Conversion"));

	// --- 2. Group and create components ---
	TMap<FHISMGroupKey, UInstancedStaticMeshComponent*> MeshMap;

	for (int32 i = 0; i < Sources.Num(); ++i)
	{
		const FSMSnapshot& Snap = Snapshots[i];
		if (!Snap.Mesh) continue;

		FHISMGroupKey Key;

		Key.Mesh = Snap.Mesh;
		Key.Materials = Snap.Materials;
		Key.Level = Sources[i]->GetOwner()->GetLevel();

		/*
		 * Keep the ORIGINAL CPD count because PerInstanceCustomData
		 * uses the original DataIndex.
		 */
		Key.CDONum = Snap.CustomDataFloats.Num();

		/*
		 * Component-level values become part of the grouping key.
		 */
		for (const int32 CPDIndex : Snap.ComponentDataIndices)
		{
			if (Snap.CustomDataFloats.IsValidIndex(CPDIndex)) { Key.ComponentLevelValues.Add(Snap.CustomDataFloats[CPDIndex]); }
			else { Key.ComponentLevelValues.Add(0.f); }
		}

		UInstancedStaticMeshComponent* ISMComp = nullptr;
		AActor* Container = nullptr;

		if (UInstancedStaticMeshComponent** Existing = MeshMap.Find(Key))
		{
			ISMComp = *Existing;
			Container = ISMComp->GetOwner();
			if (!Container)
			{
				Result.Log.Add(TEXT("[ERR] Component has no valid owner."));
				continue;
			}
		}
		else
		{
			// Spawn container actor
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			P.OverrideLevel = Key.Level;
			Container = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
			if (!Container)
			{
				Result.Log.Add(FString::Printf(TEXT("[ERR] SpawnActor failed: %s"), *Snap.Mesh->GetName()));
				continue;
			}

			Container->SetActorLabel(
			                         Cfg.bUseHISM
				                         ? FString::Printf(TEXT("HISM_%s"), *Snap.Mesh->GetName())
				                         : FString::Printf(TEXT("ISM_%s"), *Snap.Mesh->GetName())
			                        );

			for (UMaterialInterface* Mat : Snap.Materials) { EnsureMaterialSupportsInstancing(Mat, Result); }

			// Create root component
			USceneComponent* Root = NewObject<USceneComponent>(Container, TEXT("Root"));
			Root->Mobility = EComponentMobility::Static;
			Container->SetRootComponent(Root);
			Root->RegisterComponent();
			Container->AddInstanceComponent(Root);

			if (Cfg.bUseHISM)
			{
				UHierarchicalInstancedStaticMeshComponent* HISM =
					NewObject<UHierarchicalInstancedStaticMeshComponent>(
					                                                     Container,
					                                                     *FString::Printf(TEXT("HISM_%s"), *Snap.Mesh->GetName()));

				HISM->bAutoRebuildTreeOnInstanceChanges = false;
				HISM->SetMobility(EComponentMobility::Static);
				HISM->SetupAttachment(Root);
				ApplyProperties(HISM, Snap, Cfg);
				HISM->SetNumCustomDataFloats(Snap.CustomDataFloats.Num());
				HISM->RegisterComponent();
				Container->AddInstanceComponent(HISM);
				ISMComp = HISM;
			}
			else
			{
				UInstancedStaticMeshComponent* ISM =
					NewObject<UInstancedStaticMeshComponent>(
					                                         Container,
					                                         *FString::Printf(TEXT("ISM_%s"), *Snap.Mesh->GetName()));

				ISM->SetMobility(EComponentMobility::Static);
				ISM->SetupAttachment(Root);
				ApplyProperties(ISM, Snap, Cfg);
				ISM->SetNumCustomDataFloats(Snap.CustomDataFloats.Num());
				ISM->RegisterComponent();
				Container->AddInstanceComponent(ISM);
				ISMComp = ISM;
			}

			MeshMap.Add(Key, ISMComp);

			Result.ISMActorsCreated++;

			Result.Log.Add(
			               FString::Printf(
			                               TEXT("[OK] Created %s_%s"),
			                               Cfg.bUseHISM
				                               ? TEXT("HISM")
				                               : TEXT("ISM"),
			                               *Snap.Mesh->GetName()
			                              )
			              );

			/*
			 * Set component-level CPD once (visible immediately in this
			 * editor session - see note below on why that's not enough).
			 */
			for (const int32 CPDIndex : Snap.ComponentDataIndices)
			{
				if (!Snap.CustomDataFloats.IsValidIndex(CPDIndex)) { continue; }

				const float Value = Snap.CustomDataFloats[CPDIndex];

				ISMComp->SetCustomPrimitiveDataFloat(CPDIndex, Value);

				UE_LOG(
				       LogTemp,
				       Warning,
				       TEXT("[CPD -> ISM] Component CPD[%d] = %f"),
				       CPDIndex,
				       Value
				      );
			}

			// >>> NUOVO -----------------------------------------------------
			// SetCustomPrimitiveDataFloat() above is RUNTIME-ONLY and does
			// not serialize (Epic's own docs). Without this, the values
			// look correct right now in the viewport but are lost the
			// moment this level is saved & reopened, entered in PIE, or
			// packaged - exactly the same failure mode ConvertBack had.
			// Attach the helper component so there is one persistent,
			// editable source of truth, applied on every (re)registration.
			// Only attached when there is actually something to persist:
			// materials that read every CPD index via Per Instance Custom
			// Data don't need it, that data already lives in
			// PerInstanceSMCustomData, which IS serialized normally.
			if (Key.ComponentLevelValues.Num() > 0)
			{
				TArray<float> BakedValues;
				BakedValues.SetNumZeroed(Snap.CustomDataFloats.Num());

				for (int32 k = 0; k < Snap.ComponentDataIndices.Num(); ++k)
				{
					const int32 CPDIndex = Snap.ComponentDataIndices[k];
					if (BakedValues.IsValidIndex(CPDIndex)) { BakedValues[CPDIndex] = Key.ComponentLevelValues[k]; }
				}

				USMConvertedCPDComponent* CPDComp =
					NewObject<USMConvertedCPDComponent>(Container, TEXT("BakedComponentCPD"));
				CPDComp->BakedCustomPrimitiveData = BakedValues;
				CPDComp->RegisterComponent();
				Container->AddInstanceComponent(CPDComp);

				Result.Log.Add(FString::Printf(
				                               TEXT("[OK] Attached persistent CPD holder to %s (%d component-level value(s))."),
				                               *Container->GetActorLabel(), Key.ComponentLevelValues.Num()));
			}
			// <<< FINE NUOVO --------------------------------------------------
		}

		// --- 3. Add instance ---

		const int32 NumCustomDataFloats = Snap.CustomDataFloats.Num();

		const int32 InstanceIdx =
			ISMComp->AddInstance(
			                     Snap.WorldTransform,
			                     /*bWorldSpace=*/true
			                    );

		if (InstanceIdx == INDEX_NONE)
		{
			UE_LOG(
			       LogTemp,
			       Error,
			       TEXT("[CPD -> ISM] AddInstance FAILED for %s"),
			       *Snap.Mesh->GetName()
			      );

			continue;
		}

		UE_LOG(
		       LogTemp,
		       Warning,
		       TEXT("[CPD -> ISM] Added Instance=%d NumCPD=%d"),
		       InstanceIdx,
		       NumCustomDataFloats
		      );

		if (Snap.CustomDataFloats.Num() > 0)
		{
			TArray<float> PerInstanceValues;

			PerInstanceValues.SetNumZeroed(Snap.CustomDataFloats.Num());

			for (const int32 CPDIndex : Snap.PerInstanceDataIndices)
			{
				if (!Snap.CustomDataFloats.IsValidIndex(CPDIndex)) { continue; }

				PerInstanceValues[CPDIndex] = Snap.CustomDataFloats[CPDIndex];
			}

			const bool bSuccess =
				ISMComp->SetCustomData(
				                       InstanceIdx,
				                       MakeArrayView(PerInstanceValues),
				                       /*bMarkRenderStateDirty=*/false
				                      );

			UE_LOG(
			       LogTemp,
			       Warning,
			       TEXT(
				       "[CPD -> ISM] SetCustomData "
				       "Instance=%d Success=%s "
				       "PerInstanceIndices=%d"
			       ),
			       InstanceIdx,
			       bSuccess ? TEXT("TRUE") : TEXT("FALSE"),
			       Snap.PerInstanceDataIndices.Num()
			      );
		}
		ISMComp->MarkRenderStateDirty();

		Result.SourcesProcessed++;
	}

	// --- 4. Rebuild HISM BVH ---
	if (Cfg.bUseHISM)
	{
		for (auto& Pair : MeshMap)
		{
			if (UHierarchicalInstancedStaticMeshComponent* HISM =
				Cast<UHierarchicalInstancedStaticMeshComponent>(Pair.Value))
			{
				HISM->bAutoRebuildTreeOnInstanceChanges = true;
				HISM->BuildTreeIfOutdated(/*bAsync=*/false, /*bForce=*/true);
			}
		}
	}

	// --- 5. Destroy source actors ---
	if (Cfg.bDeleteSourceActors)
	{
		TSet<AActor*> ToDelete;
		for (UStaticMeshComponent* Comp : Sources)
			if (Comp && Comp->GetOwner())
				ToDelete.Add(Comp->GetOwner());

		for (AActor* Actor : ToDelete)
			if (IsValid(Actor))
			{
				Actor->Modify();
				World->DestroyActor(Actor);
			}

		Result.Log.Add(FString::Printf(TEXT("[OK] Destroyed %d source actors."), Sources.Num()));
	}

	// --- 6. Flush render state and select ---
	Sub->SelectNothing();
	for (auto& Pair : MeshMap)
	{
		if (Pair.Value)
			Pair.Value->MarkRenderStateDirty();
		if (Pair.Value && Pair.Value->GetOwner())
			Sub->SetActorSelectionState(Pair.Value->GetOwner(), true);
	}

	World->MarkPackageDirty();

	Result.Log.Add(FString::Printf(
	                               TEXT("[DONE] %d actors → %d %s(s)."),
	                               Result.SourcesProcessed,
	                               Result.ISMActorsCreated,
	                               Cfg.bUseHISM ? TEXT("HISM") : TEXT("ISM")));

	if (GEditor) GEditor->RedrawAllViewports();

	return Result;
}

// ---------------------------------------------------------------------------
// Convert Back from ISM/HISM to a StaticMeshActor
// ---------------------------------------------------------------------------
/**
 * Converts Instanced Static Mesh (ISM) or Hierarchical Instanced Static Mesh (HISM)
 * components back to individual Static Mesh Actors.
 *
 * @param Cfg The settings for the conversion process.
 * @return A result structure containing information about the conversion.
 */
FISMtoSMResult FSMtoISMConverter::ConvertBack(const FISMtoSMSettings& Cfg)
{
	FISMtoSMResult Result;

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Result.Log.Add(TEXT("[ERR] No editor world."));
		return Result;
	}

	UEditorActorSubsystem* Sub = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	if (!Sub)
	{
		Result.Log.Add(TEXT("[ERR] EditorActorSubsystem not available."));
		return Result;
	}
	TArray<AActor*> Selected = Sub->GetSelectedLevelActors();
	if (Selected.Num() == 0)
	{
		Result.Log.Add(TEXT("[WARN] No actors selected."));
		return Result;
	}

	TArray<UInstancedStaticMeshComponent*> SourceComps;
	for (AActor* Actor : Selected)
	{
		if (!Actor) continue;
		TArray<UInstancedStaticMeshComponent*> Comps;
		Actor->GetComponents<UInstancedStaticMeshComponent>(Comps);
		SourceComps.Append(Comps);
	}

	if (SourceComps.IsEmpty())
	{
		Result.Log.Add(TEXT("[WARN] No ISM/HISM component in selection."));
		return Result;
	}

	const FScopedTransaction Tx(NSLOCTEXT("SMtoISM", "Revert", "ISM to Static Mesh Revert"));

	TSet<AActor*> ContainersToCheck;

	for (UInstancedStaticMeshComponent* Comp : SourceComps)
	{
		if (!Comp || !Comp->GetStaticMesh()) continue;

		UStaticMesh* Mesh = Comp->GetStaticMesh();
		ULevel* Level = Comp->GetOwner() ? Comp->GetOwner()->GetLevel() : nullptr;
		const int32 NumInst = Comp->GetInstanceCount();
		const int32 NumCPD = Comp->NumCustomDataFloats;

		TArray<int32> PerInstanceIndices;
		TArray<int32> ComponentIndices;
		DetermineCPDDataModes(Comp, NumCPD, PerInstanceIndices, ComponentIndices);

		TSet<int32> ComponentIndexSet(ComponentIndices);
		const FCustomPrimitiveData& CompCPD = Comp->GetCustomPrimitiveData();

		UE_LOG(LogTemp, Warning, TEXT("[REVERT] Comp=%s NumCPD=%d PerInstanceArraySize=%d PerInstanceIdx=%d ComponentIdx=%d"),
		       *Comp->GetName(), NumCPD, Comp->PerInstanceSMCustomData.Num(), PerInstanceIndices.Num(), ComponentIndices.Num());

		for (int32 i = 0; i < NumInst; ++i)
		{
			FTransform InstTransform;
			if (!Comp->GetInstanceTransform(i, InstTransform, /*bWorldSpace=*/true)) continue;

			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			P.OverrideLevel = Level;
			AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), InstTransform, P);
			if (!NewActor)
			{
				Result.Log.Add(FString::Printf(TEXT("[ERR] SpawnActor failed for instance %d of %s"), i, *Mesh->GetName()));
				continue;
			}

			NewActor->SetActorLabel(FString::Printf(TEXT("%s_%d"), *Mesh->GetName(), i));

			UStaticMeshComponent* SMComp = NewActor->GetStaticMeshComponent();
			if (Cfg.bForceMovable) { SMComp->SetMobility(EComponentMobility::Movable); }
			else { SMComp->SetMobility(Comp->Mobility); }
			SMComp->SetStaticMesh(Mesh);

			for (int32 m = 0; m < Comp->GetNumMaterials(); ++m)
				SMComp->SetMaterial(m, Comp->GetMaterial(m));

			SMComp->CastShadow = Comp->CastShadow;
			SMComp->bCastDynamicShadow = Comp->bCastDynamicShadow;
			SMComp->bCastStaticShadow = Comp->bCastStaticShadow;
			SMComp->bCastContactShadow = Comp->bCastContactShadow;
			SMComp->bSelfShadowOnly = Comp->bSelfShadowOnly;
			SMComp->bReceivesDecals = Comp->bReceivesDecals;
			SMComp->bRenderCustomDepth = Comp->bRenderCustomDepth;
			SMComp->CustomDepthStencilValue = Comp->CustomDepthStencilValue;
			SMComp->BoundsScale = Comp->BoundsScale;
			SMComp->ForcedLodModel = Comp->ForcedLodModel;
			SMComp->MinLOD = Comp->MinLOD;
			SMComp->LDMaxDrawDistance = Comp->LDMaxDrawDistance;
			SMComp->bOverrideLightMapRes = Comp->bOverrideLightMapRes;
			SMComp->OverriddenLightMapRes = Comp->OverriddenLightMapRes;

			// Component-level indices come from the ISM/HISM component's own
			// CustomPrimitiveData (shared by the whole group), per-instance
			// indices come from that specific instance's slot.
			TArray<float> BakedValues;
			BakedValues.SetNumZeroed(NumCPD);

			for (int32 j = 0; j < NumCPD; ++j)
			{
				if (ComponentIndexSet.Contains(j)) { BakedValues[j] = CompCPD.Data.IsValidIndex(j) ? CompCPD.Data[j] : 0.f; }
				else
				{
					const int32 FlatIndex = i * NumCPD + j;
					BakedValues[j] = Comp->PerInstanceSMCustomData.IsValidIndex(FlatIndex)
						                 ? Comp->PerInstanceSMCustomData[FlatIndex]
						                 : 0.f;
				}
			}

			// >>> NUOVO -----------------------------------------------------
			// SetCustomPrimitiveDataFloat() is RUNTIME-ONLY and does not
			// serialize, so the values are handed ENTIRELY to the helper
			// component below - it is now the single source of truth and
			// applies them itself (OnRegister), both right now in this
			// editor session and on every future load/PIE/package. No
			// separate manual SetCustomPrimitiveDataFloat() call here
			// anymore: having two code paths write the same values was
			// exactly what made them fall out of sync when edited later.
			if (NumCPD > 0)
			{
				USMConvertedCPDComponent* CPDComp =
					NewObject<USMConvertedCPDComponent>(NewActor, TEXT("BakedCPD"));
				CPDComp->BakedCustomPrimitiveData = BakedValues;
				CPDComp->RegisterComponent();
				NewActor->AddInstanceComponent(CPDComp);
			}
			// <<< FINE NUOVO --------------------------------------------------

			Result.ActorsCreated++;
			Result.InstancesRestored++;
		}

		if (AActor* Container = Comp->GetOwner())
			ContainersToCheck.Add(Container);
	}

	if (Cfg.bDeleteSourceComponent)
	{
		for (AActor* Container : ContainersToCheck)
		{
			if (!IsValid(Container)) continue;
			Container->Modify();
			World->DestroyActor(Container);
		}
		Result.Log.Add(FString::Printf(TEXT("[OK] Destroyed %d ISM/HISM container actor(s)."), ContainersToCheck.Num()));
	}

	Sub->SelectNothing();
	World->MarkPackageDirty();

	Result.Log.Add(FString::Printf(
	                               TEXT("[DONE] %d instance(s) -> %d Static Mesh Actor(s)."),
	                               Result.InstancesRestored, Result.ActorsCreated));

	return Result;
}

// ---------------------------------------------------------------------------
// Ensure the material has 'Used with Instanced Static Meshes' enabled,
// ---------------------------------------------------------------------------
/**
 * Ensures that the given material has the 'Used with Instanced Static Meshes' property enabled.
 * If the property is not enabled, it updates the material to enable it and logs a warning.
 *
 * @param Mat The material interface to check and update.
 * @param OutResult The result object where warnings or logs are added.
 */
void FSMtoISMConverter::EnsureMaterialSupportsInstancing(UMaterialInterface* Mat, FSMtoISMResult& OutResult)
{
	if (!Mat) return;

	// Retrieve the base material from the material interface.
	if (UMaterial* Base = Mat->GetMaterial())
	{
		// Check if the material is not configured for instanced static meshes.
		if (!Base->bUsedWithInstancedStaticMeshes)
		{
			// Log a warning about the material configuration.
			OutResult.Log.Add(FString::Printf(
			                                  TEXT("[WARN] '%s' non ha 'Used with Instanced Static Meshes' - rischia di renderizzare sbagliato sull'ISM/HISM."),
			                                  *Base->GetName()));

			// Enable the 'Used with Instanced Static Meshes' property.
			Base->bUsedWithInstancedStaticMeshes = true;

			// Notify the editor about the material change.
			Base->PostEditChange();
		}
	}
}

// ---------------------------------------------------------------------------
// Material Per Instance Custom Data validation
// ---------------------------------------------------------------------------
/**
 * Checks if a given material contains any Per Instance Custom Data expressions and optionally retrieves their indices.
 *
 * @param Material The material interface to check.
 * @param OutDataIndices An optional array to store the indices of found Per Instance Custom Data expressions.
 * @return True if the material contains any Per Instance Custom Data expressions, false otherwise.
 */
bool FSMtoISMConverter::MaterialHasPerInstanceCustomData(
	UMaterialInterface* Material,
	TArray<int32>* OutDataIndices)
{
	if (OutDataIndices) { OutDataIndices->Reset(); }

	if (!Material) { return false; }

	UMaterial* BaseMaterial = Material->GetMaterial();

	if (!BaseMaterial) { return false; }

	bool bFound = false;

	for (UMaterialExpression* Expression : BaseMaterial->GetExpressions())
	{
		if (!Expression) { continue; }

		if (const UMaterialExpressionPerInstanceCustomData* CustomData =
			Cast<UMaterialExpressionPerInstanceCustomData>(Expression))
		{
			bFound = true;

			if (OutDataIndices)
			{
				OutDataIndices->AddUnique(
				                          static_cast<int32>(CustomData->DataIndex)
				                         );
			}

			continue;
		}

		if (const UMaterialExpressionPerInstanceCustomData3Vector* CustomData3 =
			Cast<UMaterialExpressionPerInstanceCustomData3Vector>(Expression))
		{
			bFound = true;

			if (OutDataIndices)
			{
				OutDataIndices->AddUnique(
				                          static_cast<int32>(CustomData3->DataIndex)
				                         );
			}
		}
	}

	return bFound;
}

// ---------------------------------------------------------------------------
// Validate a material against the CPD indices that will actually be copied
// ---------------------------------------------------------------------------
/**
 * Validates whether a given material contains all required Custom Primitive Data (CPD) indices.
 *
 * @param Material The material interface to validate.
 * @param RequiredIndices The set of required CPD indices that must be present in the material.
 * @param OutMissingIndices An array that will be filled with any missing CPD indices found in the material.
 * @return True if the material contains all required CPD indices, false if any are missing.
 */
bool FSMtoISMConverter::ValidateMaterialCustomData(
	UMaterialInterface* Material,
	const TSet<int32>& RequiredIndices,
	TArray<int32>& OutMissingIndices)
{
	OutMissingIndices.Reset();

	if (!Material) { return true; }

	if (RequiredIndices.Num() == 0) { return true; }

	TArray<int32> MaterialDataIndices;

	MaterialHasPerInstanceCustomData(
	                                 Material,
	                                 &MaterialDataIndices
	                                );

	TSet<int32> MaterialIndices;

	for (const int32 Index : MaterialDataIndices) { MaterialIndices.Add(Index); }

	bool bValid = true;

	for (const int32 RequiredIndex : RequiredIndices)
	{
		if (!MaterialIndices.Contains(RequiredIndex))
		{
			OutMissingIndices.Add(RequiredIndex);
			bValid = false;
		}
	}

	OutMissingIndices.Sort();

	return bValid;
}

// ---------------------------------------------------------------------------
// Validate all materials used by an Actor
// ---------------------------------------------------------------------------
/**
 * Validates the materials of a given actor against the required Custom Primitive Data (CPD) indices.
 *
 * @param Actor The actor whose materials are to be validated.
 * @param RequiredIndices The set of required CPD indices that must be present in the materials.
 * @param OutWarnings An array that will be filled with warning messages for any validation issues found.
 * @return True if all materials are valid, false if any issues were found.
 */
bool FSMtoISMConverter::ValidateActorMaterials(
	AActor* Actor,
	const TSet<int32>& RequiredIndices,
	TArray<FString>& OutWarnings)
{
	if (!Actor) { return true; }

	if (RequiredIndices.Num() == 0) { return true; }

	TArray<UStaticMeshComponent*> Components;

	Actor->GetComponents<UStaticMeshComponent>(Components);

	bool bAllValid = true;

	UE_LOG(
	       LogTemp,
	       Warning,
	       TEXT(
		       "[SMConverter] VALIDATING Actor='%s' Components=%d RequiredCPD=%d"
	       ),
	       *Actor->GetActorLabel(),
	       Components.Num(),
	       RequiredIndices.Num()
	      );

	for (UStaticMeshComponent* Component : Components)
	{
		if (!Component) { continue; }

		if (Component->IsA<UInstancedStaticMeshComponent>()) { continue; }

		if (!Component->GetStaticMesh()) { continue; }

		UE_LOG(
		       LogTemp,
		       Warning,
		       TEXT(
			       "[SMConverter] Checking Component='%s' Materials=%d"
		       ),
		       *Component->GetName(),
		       Component->GetNumMaterials()
		      );

		for (int32 MaterialIndex = 0;
		     MaterialIndex < Component->GetNumMaterials();
		     ++MaterialIndex)
		{
			UMaterialInterface* Material =
				Component->GetMaterial(MaterialIndex);

			if (!Material)
			{
				UE_LOG(
				       LogTemp,
				       Warning,
				       TEXT(
					       "[SMConverter] Component='%s' Material[%d] = NULL"
				       ),
				       *Component->GetName(),
				       MaterialIndex
				      );

				continue;
			}

			TArray<int32> MissingIndices;

			const bool bValid =
				ValidateMaterialCustomData(
				                           Material,
				                           RequiredIndices,
				                           MissingIndices
				                          );

			if (bValid)
			{
				UE_LOG(
				       LogTemp,
				       Warning,
				       TEXT(
					       "[SMConverter] OK Actor='%s' Material='%s'"
				       ),
				       *Actor->GetActorLabel(),
				       *Material->GetName()
				      );

				continue;
			}

			bAllValid = false;

			FString MissingString;

			for (int32 i = 0;
			     i < MissingIndices.Num();
			     ++i)
			{
				if (i > 0) { MissingString += TEXT(", "); }

				MissingString +=
					FString::FromInt(MissingIndices[i]);
			}

			const FString Warning = FString::Printf(
			                                        TEXT(
			                                             "[SMConverter] WARNING: "
			                                             "Actor='%s' "
			                                             "Component='%s' "
			                                             "Material='%s' "
			                                             "is missing Per Instance Custom Data "
			                                             "for CPD indices [%s]"
			                                            ),
			                                        *Actor->GetActorLabel(),
			                                        *Component->GetName(),
			                                        *Material->GetName(),
			                                        *MissingString
			                                       );

			OutWarnings.Add(Warning);

			UE_LOG(
			       LogTemp,
			       Warning,
			       TEXT("%s"),
			       *Warning
			      );
		}
	}

	return bAllValid;
}

// ---------------------------------------------------------------------------
// Validate all selected Actors
// ---------------------------------------------------------------------------
/**
 * Validates the materials of all selected actors against the required Custom Primitive Data (CPD) indices.
 *
 * @param Actors The array of actors to validate.
 * @param RequiredIndices The set of required CPD indices that must be present in the materials.
 * @param OutWarnings An array that will be filled with warning messages for any validation issues found.
 * @return True if all materials are valid, false if any issues were found.
 */
bool FSMtoISMConverter::ValidateSelectedMaterials(
	const TArray<AActor*>& Actors,
	const TSet<int32>& RequiredIndices,
	TArray<FString>& OutWarnings)
{
	OutWarnings.Reset();

	UE_LOG(
	       LogTemp,
	       Warning,
	       TEXT(
		       "[SMConverter] ===== MATERIAL VALIDATION ====="
	       )
	      );

	UE_LOG(
	       LogTemp,
	       Warning,
	       TEXT(
		       "[SMConverter] Actors=%d RequiredCPDIndices=%d"
	       ),
	       Actors.Num(),
	       RequiredIndices.Num()
	      );

	for (const int32 Index : RequiredIndices)
	{
		UE_LOG(
		       LogTemp,
		       Warning,
		       TEXT(
			       "[SMConverter] Required CPD Index=%d"
		       ),
		       Index
		      );
	}

	if (RequiredIndices.Num() == 0)
	{
		UE_LOG(
		       LogTemp,
		       Warning,
		       TEXT(
			       "[SMConverter] No CPD indices to validate."
		       )
		      );

		return true;
	}

	bool bAllValid = true;

	for (AActor* Actor : Actors)
	{
		if (!Actor) { continue; }

		if (!ValidateActorMaterials(
		                            Actor,
		                            RequiredIndices,
		                            OutWarnings)) { bAllValid = false; }
	}

	UE_LOG(
	       LogTemp,
	       Warning,
	       TEXT(
		       "[SMConverter] ===== MATERIAL VALIDATION END ===== "
		       "Valid=%s Warnings=%d"
	       ),
	       bAllValid ? TEXT("TRUE") : TEXT("FALSE"),
	       OutWarnings.Num()
	      );

	return bAllValid;
}

// ---------------------------------------------------------------------------
// Determine which CPD indices are PerInstance and which are Component-level
// ---------------------------------------------------------------------------
/**
 * Determines which Custom Primitive Data (CPD) indices are PerInstance and which are Component-level for a given Static Mesh Component.
 *
 * @param Component The Static Mesh Component to analyze.
 * @param NumCPD The total number of CPD indices to evaluate.
 * @param OutPerInstanceIndices An array that will be filled with the indices that are PerInstance.
 * @param OutComponentIndices An array that will be filled with the indices that are Component-level.
 */
void FSMtoISMConverter::DetermineCPDDataModes(UStaticMeshComponent* Component, int32 NumCPD, TArray<int32>& OutPerInstanceIndices,
                                              TArray<int32>& OutComponentIndices)
{
	OutPerInstanceIndices.Reset();
	OutComponentIndices.Reset();

	if (!Component || NumCPD <= 0) { return; }

	/*
	 * Start assuming that every CPD index is component-level.
	 *
	 * An index becomes PerInstance only if ALL materials contain
	 * a Per Instance Custom Data node for that exact index.
	 */
	TArray<TSet<int32>> MaterialPICDIndices;

	const int32 NumMaterials = Component->GetNumMaterials();

	for (int32 MaterialIndex = 0;
	     MaterialIndex < NumMaterials;
	     ++MaterialIndex)
	{
		UMaterialInterface* Material =
			Component->GetMaterial(MaterialIndex);

		TSet<int32> Indices;

		if (Material)
		{
			TArray<int32> FoundIndices;

			FSMtoISMConverter::MaterialHasPerInstanceCustomData(
			                                                    Material,
			                                                    &FoundIndices
			                                                   );

			for (const int32 Index : FoundIndices) { Indices.Add(Index); }
		}

		MaterialPICDIndices.Add(MoveTemp(Indices));
	}

	/*
	 * Evaluate every CPD index independently.
	 */
	for (int32 CPDIndex = 0;
	     CPDIndex < NumCPD;
	     ++CPDIndex)
	{
		bool bPerInstanceForAllMaterials = true;

		/*
		 * If there are no material slots, treat the data as
		 * component-level.
		 */
		if (MaterialPICDIndices.Num() == 0) { bPerInstanceForAllMaterials = false; }

		for (const TSet<int32>& MaterialIndices : MaterialPICDIndices)
		{
			if (!MaterialIndices.Contains(CPDIndex))
			{
				bPerInstanceForAllMaterials = false;
				break;
			}
		}

		if (bPerInstanceForAllMaterials) { OutPerInstanceIndices.Add(CPDIndex); }
		else { OutComponentIndices.Add(CPDIndex); }
	}
}
