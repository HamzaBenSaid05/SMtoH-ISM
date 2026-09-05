#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SMtoISMBlueprintLibrary.generated.h"

/**
 * Settings structure for converting Static Meshes to Instanced Static Meshes (ISM).
 */
USTRUCT(BlueprintType)
struct FSMtoISMSettingsBP
{	
	GENERATED_BODY()

	/** Whether to read data from the source actors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behaviour")
	bool bReadFromSource = true;

	/** Whether to delete the source actors after conversion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behaviour")
	bool bDeleteSourceActors = true;

	/**
	 * Indicates whether to use Hierarchical Instanced Static Meshes (HISM) or simple Instanced Static Meshes (ISM).
	 * 
	 * - true: Use HISM, which includes BVH (Bounding Volume Hierarchy) culling. 
	 *         This is optimal for many small objects like rocks or vegetation.
	 * - false: Use simple ISM, which does not include BVH. 
	 *          This is better suited for fewer large objects like dragons or buildings.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behaviour")
	bool bUseHISM = true;

	/** Whether the ISM should cast shadows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shadow")
	bool bCastShadow = true;

	/** Whether the ISM should cast dynamic shadows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shadow")
	bool bCastDynamicShadow = true;

	/** Whether the ISM should cast static shadows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shadow")
	bool bCastStaticShadow = true;

	/** Whether the ISM should cast contact shadows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shadow")
	bool bCastContactShadow = false;

	/** Whether the ISM should cast shadows only on itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shadow")
	bool bSelfShadowOnly = false;

	/** Whether the ISM should receive decals. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rendering")
	bool bReceivesDecals = true;

	/** Whether the ISM should render in the custom depth pass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rendering")
	bool bRenderCustomDepth = false;

	/** Custom depth stencil value for the ISM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rendering")
	int32 CustomDepthStencil = 0;

	/** Scale factor for the bounds of the ISM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rendering")
	float BoundsScale = 1.0f;

	/** Forced Level of Detail (LOD) for the ISM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LOD")
	int32 ForcedLOD = 0;

	/** Minimum Level of Detail (LOD) for the ISM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LOD")
	int32 MinLOD = 0;

	/** Maximum draw distance for the ISM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LOD")
	float MaxDrawDistance = 0.f;

	/** Whether to override the lightmap resolution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lightmap")
	bool bOverrideLightmapRes = false;

	/** Resolution to use for the lightmap if overridden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lightmap")
	int32 OverrideLightmapRes = 64;

	/** UV channel to use for the lightmap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lightmap")
	int32 LightmapUVChannel = 1;
};

/**
 * Result structure for converting Static Meshes to Instanced Static Meshes (ISM).
 */
USTRUCT(BlueprintType)
struct FSMtoISMResultBP
{
	GENERATED_BODY()

	/** Number of ISM actors created during the conversion. */
	UPROPERTY(BlueprintReadOnly, Category="Result")
	int32 ISMActorsCreated = 0;

	/** Number of source actors processed during the conversion. */
	UPROPERTY(BlueprintReadOnly, Category="Result")
	int32 SourcesProcessed = 0;

	/** Log messages generated during the conversion process. */
	UPROPERTY(BlueprintReadOnly, Category="Result")
	TArray<FString> Log;
};

/**
 * Settings structure for reverting Instanced Static Meshes (ISM) to Static Meshes.
 */
USTRUCT(BlueprintType)
struct FISMtoSMSettingsBP
{
	GENERATED_BODY()

	/** Whether to delete the source ISM component after reverting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behaviour")
	bool bDeleteSourceComponent = true;

	/** Whether to force the reverted Static Mesh to be movable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behaviour")
	bool bForceMovable = true;
};

/**
 * Result structure for reverting Instanced Static Meshes (ISM) to Static Meshes.
 */
USTRUCT(BlueprintType)
struct FISMtoSMResultBP
{
	GENERATED_BODY()

	/** Number of actors created during the reversion process. */
	UPROPERTY(BlueprintReadOnly, Category="Result")
	int32 ActorsCreated = 0;

	/** Number of instances restored during the reversion process. */
	UPROPERTY(BlueprintReadOnly, Category="Result")
	int32 InstancesRestored = 0;

	/** Log messages generated during the reversion process. */
	UPROPERTY(BlueprintReadOnly, Category="Result")
	TArray<FString> Log;
};

/**
 * Blueprint library for converting between Static Meshes and Instanced Static Meshes (ISM).
 */
UCLASS()
class SMCONVERTER_API USMtoISMBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Converts Static Mesh actors to Instanced Static Meshes (ISM).
	 *
	 * @param Settings The settings for the conversion process.
	 * @param Result The result of the conversion process.
	 */
	UFUNCTION(BlueprintCallable, Category="SM to ISM",
		meta=(DisplayName="Convert Static Mesh To ISM"))
	static void ConvertStaticMeshToISM(
		const FSMtoISMSettingsBP Settings,
		FSMtoISMResultBP& Result);

	/**
	 * Reverts Instanced Static Meshes (ISM) to Static Mesh actors.
	 *	
	 * @param Settings The settings for the reversion process.
	 * @param Result The result of the reversion process.
	 */
	UFUNCTION(BlueprintCallable, Category="SM to ISM",
		meta=(DisplayName="Revert ISM To Static Mesh Actors"))
	static void RevertISMToStaticMeshActors(
		const FISMtoSMSettingsBP Settings,
		FISMtoSMResultBP& Result);
};
