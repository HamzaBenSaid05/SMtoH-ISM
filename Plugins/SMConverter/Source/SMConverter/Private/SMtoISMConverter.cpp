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
#include "SMtoISMMaterialCompatibility.h"

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

	const FCustomPrimitiveData& CPD = C->GetCustomPrimitiveData();
	const int32 NumCPD = CPD.Data.Num();
	S.CustomDataFloats.SetNum(NumCPD);
	for (int32 i = 0; i < NumCPD; ++i)
		S.CustomDataFloats[i] = CPD.Data[i];

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
	const TArray<UMaterialInterface*>& Materials,
	const FSMtoISMSettings& Cfg)
{
	C->SetStaticMesh(Src.Mesh);

	for (int32 i = 0; i < Materials.Num(); ++i) { C->SetMaterial(i, Materials[i]); }

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

	// Commutative sum: material order does not matter
	uint32 MatHash = 0;
	for (UMaterialInterface* Mat : K.Materials)
		MatHash += ::GetTypeHash(Mat);
	Hash = HashCombine(Hash, MatHash);

	return Hash;
}

// ---------------------------------------------------------------------------
/**
 * Converts Static Mesh Components to Instanced Static Mesh Components.
 *
 * @param Cfg The settings for the conversion process.
 * @return The result of the conversion process.
 */
FSMtoISMResult FSMtoISMConverter::Convert(const FSMtoISMSettings& Cfg)
{
	FSMtoISMResult Result;

	FSMtoISMMaterialProxyContext MaterialContext;
	MaterialContext.Log = &Result.Log;

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Result.Log.Add(TEXT("[ERR] No editor world."));
		return Result;
	}

	UEditorActorSubsystem* Sub = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
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

		TArray<UMaterialInterface*> ISMMaterials;
		ISMMaterials.Reserve(Snap.Materials.Num());

		for (UMaterialInterface* SourceMaterial : Snap.Materials)
		{
			UMaterialInterface* ISMMaterial =
				FSMtoISMMaterialCompatibility::
				PrepareMaterialForISM(
				                      SourceMaterial,
				                      MaterialContext
				                     );

			ISMMaterials.Add(ISMMaterial);
		}

		FHISMGroupKey Key;
		Key.Mesh = Snap.Mesh;
		Key.Materials = ISMMaterials;
		Key.Level = Sources[i]->GetOwner()->GetLevel();
		Key.CDONum = Snap.CustomDataFloats.Num();

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

			Container->SetActorLabel(FString::Printf(TEXT("ISM_%s"), *Snap.Mesh->GetName()));

			// Create root component
			USceneComponent* Root = NewObject<USceneComponent>(Container, TEXT("Root"));
			Root->Mobility = EComponentMobility::Static;
			Root->RegisterComponent();
			Container->SetRootComponent(Root);

			if (Cfg.bUseHISM)
			{
				// Create HISM component
				UHierarchicalInstancedStaticMeshComponent* HISM =
					NewObject<UHierarchicalInstancedStaticMeshComponent>(
					                                                     Container,
					                                                     *FString::Printf(TEXT("HISM_%s"), *Snap.Mesh->GetName()));

				HISM->bAutoRebuildTreeOnInstanceChanges = false;
				HISM->SetMobility(EComponentMobility::Static);
				HISM->SetupAttachment(Root);
				Container->AddInstanceComponent(HISM);
				ApplyProperties(HISM, Snap,ISMMaterials, Cfg);
				HISM->SetNumCustomDataFloats(Snap.CustomDataFloats.Num());
				HISM->RegisterComponent();
				ISMComp = HISM;
			}
			else
			{
				// Create ISM component
				UInstancedStaticMeshComponent* ISM =
					NewObject<UInstancedStaticMeshComponent>(
					                                         Container,
					                                         *FString::Printf(TEXT("ISM_%s"), *Snap.Mesh->GetName()));

				ISM->SetMobility(EComponentMobility::Static);
				ISM->SetupAttachment(Root);
				Container->AddInstanceComponent(ISM);
				ApplyProperties(ISM, Snap, ISMMaterials, Cfg);
				ISM->SetNumCustomDataFloats(Snap.CustomDataFloats.Num());
				ISM->RegisterComponent();
				ISMComp = ISM;
			}

			MeshMap.Add(Key, ISMComp);
			Result.ISMActorsCreated++;
			Result.Log.Add(FString::Printf(TEXT("[OK] Created %s_%s"),
			                               Cfg.bUseHISM ? TEXT("HISM") : TEXT("ISM"),
			                               *Snap.Mesh->GetName()));
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

		// Set ALL custom data for this instance in one call.
		if (NumCustomDataFloats > 0)
		{
			const bool bSuccess =
				ISMComp->SetCustomData(
				                       InstanceIdx,
				                       MakeArrayView(Snap.CustomDataFloats),
				                       /*bMarkRenderStateDirty=*/false
				                      );

			UE_LOG(
			       LogTemp,
			       Warning,
			       TEXT("[CPD -> ISM] SetCustomData Instance=%d Success=%s"),
			       InstanceIdx,
			       bSuccess ? TEXT("TRUE") : TEXT("FALSE")
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
 * components back into individual Static Mesh Actors.
 *
 * @param Cfg The settings for the conversion process.
 * @return The result of the conversion process, including logs and statistics.
 */
FISMtoSMResult FSMtoISMConverter::ConvertBack(const FISMtoSMSettings& Cfg)
{
	FISMtoSMResult Result;

	// Retrieve the editor world context.
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Result.Log.Add(TEXT("[ERR] No editor world."));
		return Result;
	}

	// Get the selected actors in the editor.
	UEditorActorSubsystem* Sub = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	TArray<AActor*> Selected = Sub->GetSelectedLevelActors();

	// --- 1. Collect ISM/HISM components from the selection ---
	// UInstancedStaticMeshComponent also includes HISM (inherits from ISM),
	// so a single GetComponents call is sufficient for both cases.
	TArray<UInstancedStaticMeshComponent*> SourceComps;
	for (AActor* Actor : Selected)
	{
		if (!Actor) continue;
		TArray<UInstancedStaticMeshComponent*> Comps;
		Actor->GetComponents<UInstancedStaticMeshComponent>(Comps);
		SourceComps.Append(Comps);
	}

	// If no ISM/HISM components are found, log a warning and return.
	if (SourceComps.IsEmpty())
	{
		Result.Log.Add(TEXT("[WARN] No ISM/HISM component in selection."));
		return Result;
	}

	// Begin a scoped transaction for undo/redo support.
	const FScopedTransaction Tx(NSLOCTEXT("SMtoISM", "Revert", "ISM to Static Mesh Revert"));

	TSet<AActor*> ContainersToCheck;

	// --- 2. For each instance, spawn a movable AStaticMeshActor with the same properties ---
	for (UInstancedStaticMeshComponent* Comp : SourceComps)
	{
		if (!Comp || !Comp->GetStaticMesh()) continue;

		UStaticMesh* Mesh = Comp->GetStaticMesh();
		ULevel* Level = Comp->GetOwner() ? Comp->GetOwner()->GetLevel() : nullptr;
		const int32 NumInst = Comp->GetInstanceCount();
		const int32 NumCPD = Comp->NumCustomDataFloats;

		UE_LOG(LogTemp, Warning, TEXT("[REVERT] Comp=%s NumCPD=%d PerInstanceArraySize=%d"),
		       *Comp->GetName(), NumCPD, Comp->PerInstanceSMCustomData.Num());

		for (int32 i = 0; i < NumInst; ++i)
		{
			FTransform InstTransform;
			if (!Comp->GetInstanceTransform(i, InstTransform, /*bWorldSpace=*/true)) continue;

			// Spawn a new Static Mesh Actor for each instance.
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			P.OverrideLevel = Level;
			AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), InstTransform, P);
			if (!NewActor)
			{
				Result.Log.Add(FString::Printf(TEXT("[ERR] SpawnActor failed for instance %d of %s"), i, *Mesh->GetName()));
				continue;
			}

			// Set the actor label for the new Static Mesh Actor.
			NewActor->SetActorLabel(FString::Printf(TEXT("%s_%d"), *Mesh->GetName(), i));

			// Configure the Static Mesh Component of the new actor.
			UStaticMeshComponent* SMComp = NewActor->GetStaticMeshComponent();
			if (Cfg.bForceMovable) { SMComp->SetMobility(EComponentMobility::Movable); }
			else { SMComp->SetMobility(Comp->Mobility); }
			SMComp->SetStaticMesh(Mesh);

			// Copy materials from the ISM/HISM component.
			for (int32 m = 0; m < Comp->GetNumMaterials(); ++m)
				SMComp->SetMaterial(m, Comp->GetMaterial(m));

			// Copy other properties from the ISM/HISM component.
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

			// Restore per-instance custom data as custom primitive data on the new component.
			for (int32 j = 0; j < NumCPD; ++j)
			{
				const int32 FlatIndex = i * NumCPD + j;
				const float Value = Comp->PerInstanceSMCustomData.IsValidIndex(FlatIndex)
					                    ? Comp->PerInstanceSMCustomData[FlatIndex]
					                    : 0.f;
				SMComp->SetCustomPrimitiveDataFloat(j, Value);
			}

			Result.ActorsCreated++;
			Result.InstancesRestored++;
		}

		// Add the container actor to the set for potential deletion.
		if (AActor* Container = Comp->GetOwner())
			ContainersToCheck.Add(Container);
	}

	// --- 3. Delete the source ISM/HISM container actors ---
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

	// Deselect all actors and mark the world package as dirty.
	Sub->SelectNothing();
	World->MarkPackageDirty();

	// Log the results of the conversion process.
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
