// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;
class ULevel;
class UStaticMeshComponent;
class AActor;

/**
 * Settings structure for converting Static Meshes to Instanced Static Meshes (ISM).
 */
struct FSMtoISMSettings
{
	/** Whether to read data from the source actors. */
	bool bReadFromSource = true;

	/** Whether to delete the source actors after conversion. */
	bool bDeleteSourceActors = true;

	/**
	 * Indicates whether to use Hierarchical Instanced Static Meshes (HISM) or simple Instanced Static Meshes (ISM).
	 *
	 * - true: Use HISM, which includes BVH (Bounding Volume Hierarchy) culling.
	 *         This is optimal for many small objects like rocks or vegetation.
	 * - false: Use simple ISM, which does not include BVH.
	 *          This is better suited for fewer large objects like dragons or buildings.
	 */
	bool bUseHISM = true;

	// --- Shadow settings ---
	/** Whether the ISM should cast shadows. */
	bool bCastShadow = true;

	/** Whether the ISM should cast dynamic shadows. */
	bool bCastDynamicShadow = true;

	/** Whether the ISM should cast static shadows. */
	bool bCastStaticShadow = true;

	/** Whether the ISM should cast contact shadows. */
	bool bCastContactShadow = false;

	/** Whether the ISM should cast shadows only on itself. */
	bool bSelfShadowOnly = false;

	// --- Rendering settings ---
	/** Whether the ISM should receive decals. */
	bool bReceivesDecals = true;

	/** Whether the ISM should render in the custom depth pass. */
	bool bRenderCustomDepth = false;

	/** Custom depth stencil value for the ISM. */
	int32 CustomDepthStencil = 0;

	/** Scale factor for the bounds of the ISM. */
	float BoundsScale = 1.0f;

	// --- Level of Detail (LOD) settings ---
	/** Forced Level of Detail (LOD) for the ISM. */
	int32 ForcedLOD = 0;

	/** Minimum Level of Detail (LOD) for the ISM. */
	int32 MinLOD = 0;

	/** Maximum draw distance for the ISM. */
	float MaxDrawDistance = 0.f;

	// --- Lightmap settings ---
	/** Whether to override the lightmap resolution. */
	bool bOverrideLightmapRes = false;

	/** Resolution to use for the lightmap if overridden. */
	int32 OverrideLightmapRes = 64;

	/** UV channel to use for the lightmap. */
	int32 LightmapUVChannel = 1;
};

/**
 * Result structure for converting Static Meshes to Instanced Static Meshes (ISM).
 */
struct FSMtoISMResult
{
	/** Number of ISM actors created during the conversion. */
	int32 ISMActorsCreated = 0;

	/** Number of source actors processed during the conversion. */
	int32 SourcesProcessed = 0;

	/** Log messages generated during the conversion process. */
	TArray<FString> Log;
};

/**
 * Settings structure for reverting Instanced Static Meshes (ISM) to Static Meshes.
 */
struct FISMtoSMSettings
{
	/**
	 * Whether to delete the source ISM component after reverting.
	 * This includes the container actor.
	 */
	bool bDeleteSourceComponent = true;

	/**
	 * Whether to force the reverted Static Mesh to be movable.
	 * If false, the mobility of the ISM/HISM component is retained.
	 */
	bool bForceMovable = true;
};

/**
 * Result structure for reverting Instanced Static Meshes (ISM) to Static Meshes.
 */
struct FISMtoSMResult
{
	/** Number of actors created during the reversion process. */
	int32 ActorsCreated = 0;

	/** Number of instances restored during the reversion process. */
	int32 InstancesRestored = 0;

	/** Log messages generated during the reversion process. */
	TArray<FString> Log;
};

/**
 * Key structure for grouping Hierarchical Instanced Static Meshes (HISM).
 */
struct FHISMGroupKey
{
	/** Pointer to the Static Mesh used in the group. */
	UStaticMesh* Mesh = nullptr;

	/** Array of materials used in the group. */
	TArray<UMaterialInterface*> Materials;

	/** Pointer to the level where the group resides. */
	ULevel* Level = nullptr;

	/** Custom Data Object (CDO) number for the group. */
	int32 CDONum = 0;

	/** Array of component-level values for the group. */
	TArray<float> ComponentLevelValues;

	/**
	 * Equality operator for comparing two HISM group keys.
	 *
	 * @param Other The other group key to compare.
	 * @return True if the keys are equal, false otherwise.
	 */
	bool operator==(const FHISMGroupKey& Other) const
	{
		if (Mesh != Other.Mesh) return false;
		if (Level != Other.Level) return false;
		if (CDONum != Other.CDONum) return false;
		if (Materials.Num() != Other.Materials.Num()) return false;

		// Compare materials in an order-independent manner
		TArray<UMaterialInterface*> A = Materials;
		TArray<UMaterialInterface*> B = Other.Materials;
		A.Sort([](UMaterialInterface& X, UMaterialInterface& Y) { return &X < &Y; });
		B.Sort([](UMaterialInterface& X, UMaterialInterface& Y) { return &X < &Y; });
		for (int32 i = 0; i < A.Num(); ++i)
			if (A[i] != B[i]) return false;
		if (ComponentLevelValues.Num() != Other.ComponentLevelValues.Num()) return false;
		for (int32 i = 0; i < ComponentLevelValues.Num(); ++i)
			if (!FMath::IsNearlyEqual(ComponentLevelValues[i], Other.ComponentLevelValues[i]))
				return false;
		return true;
	}
};

/**
 * Hash function for the HISM group key.
 *
 * @param K The group key to hash.
 * @return The hash value of the group key.
 */
FORCEINLINE uint32 GetTypeHash(const FHISMGroupKey& K);

/**
 * Converter class for managing conversions between Static Meshes and Instanced Static Meshes (ISM).
 */
class SMCONVERTER_API FSMtoISMConverter
{
public:
	/**
	 * Converts Static Mesh actors to Instanced Static Meshes (ISM).
	 *
	 * @param Settings The settings for the conversion process.
	 * @return The result of the conversion process.
	 */
	static FSMtoISMResult Convert(
		const FSMtoISMSettings& Settings = FSMtoISMSettings()
	);

	/**
	 * Reverts Instanced Static Meshes (ISM) to Static Mesh actors.
	 *
	 * @param Settings The settings for the reversion process.
	 * @return The result of the reversion process.
	 */
	static FISMtoSMResult ConvertBack(
		const FISMtoSMSettings& Settings = FISMtoSMSettings()
	);

	/**
	 * Ensures that the given material supports instancing.
	 *
	 * @param Mat The material to check.
	 * @param OutResult The result structure to store any log messages.
	 */
	static void EnsureMaterialSupportsInstancing(
		UMaterialInterface* Mat,
		FSMtoISMResult& OutResult
	);

	static bool MaterialHasPerInstanceCustomData(
		UMaterialInterface* Material,
		TArray<int32>* OutDataIndices = nullptr
	);

	static bool ValidateMaterialCustomData(
		UMaterialInterface* Material,
		const TSet<int32>& RequiredIndices,
		TArray<int32>& OutMissingIndices
	);

	static bool ValidateActorMaterials(
		AActor* Actor,
		const TSet<int32>& RequiredIndices,
		TArray<FString>& OutWarnings
	);

	static bool ValidateSelectedMaterials(
		const TArray<AActor*>& Actors,
		const TSet<int32>& RequiredIndices,
		TArray<FString>& OutWarnings
	);

	static void DetermineCPDDataModes(
		UStaticMeshComponent* Component,
		int32 NumCPD,
		TArray<int32>& OutPerInstanceIndices,
		TArray<int32>& OutComponentIndices);
};
