// Copyright ASKD Games


#include "Volume/AkdGlobalPostProcessVolume.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AAkdGlobalPostProcessVolume::AAkdGlobalPostProcessVolume()
{
	bUnbound = true;

	// Lowest priority: a deliberate hand-placed local volume (e.g. one
	// special puzzle room with a unique look) can still win by using a
	// higher Priority. We are the floor, not the ceiling.
	Priority = -1000.0f;
	BlendWeight = 1.0f;

	// Adjust these two paths to match your actual material locations.
	CrushShadowStencilMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/PostProcess/Materials/PP_CrushShadowStencil.PP_CrushShadowStencil")));

	NeonPreservationMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/PostProcess/Materials/PP_NeonPreservation.PP_NeonPreservation")));

	ConfigureBaselineSettings();
	ConfigureBlendables();
}

AAkdGlobalPostProcessVolume* AAkdGlobalPostProcessVolume::EnsureExists(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AAkdGlobalPostProcessVolume> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient; // Never written into the level asset.

	AAkdGlobalPostProcessVolume* Volume = World->SpawnActor<AAkdGlobalPostProcessVolume>(
		AAkdGlobalPostProcessVolume::StaticClass(), FTransform::Identity, SpawnParams);

#if !UE_BUILD_SHIPPING
	if (Volume)
	{
		UE_LOG(LogTemp, Log, TEXT("AkdGlobalPostProcessVolume: spawned for level '%s'."), *World->GetMapName());
	}
#endif

	return Volume;
}

void AAkdGlobalPostProcessVolume::ConfigureBlendables()
{
	Settings.WeightedBlendables.Array.Reset();

	auto AddBlendable = [this](const TSoftObjectPtr<UMaterialInterface>& SoftMat)
		{
			if (UMaterialInterface* Mat = SoftMat.LoadSynchronous())
			{
				FWeightedBlendable Entry;
				Entry.Object = Mat;
				Entry.Weight = 1.0f; // Always on; gating happens inside the material via WorldBlendAlpha.
				Settings.WeightedBlendables.Array.Add(Entry);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("AkdGlobalPostProcessVolume: failed to load blendable '%s' - check the asset path."),
					*SoftMat.ToString());
			}
		};

	// Order matters: stencil-2 shadow darkening composites first, then
	// stencil-1 neon preservation re-brightens tagged pixels on top.
	AddBlendable(CrushShadowStencilMaterial);
	AddBlendable(NeonPreservationMaterial);
}

void AAkdGlobalPostProcessVolume::ConfigureBaselineSettings()
{
	FPostProcessSettings& PP = Settings;

	// --- Exposure ---
	// Manual: UkdWorldColorDriver controls perceived brightness via
	// LumenColor/SolarColor and EdgePulseAlpha bursts. Auto-exposure would
	// fight the driver and flicker during Crush transitions.

	//PP.bOverride_AutoExposureMethod = true;
	//PP.AutoExposureMethod = AEM_Manual;

	//PP.bOverride_AutoExposureBias = true;
	//PP.AutoExposureBias = 0.0f;

	// --- Ambient Occlusion ---
	// Geometry-driven grounding, identical in both modes.
	PP.bOverride_AmbientOcclusionIntensity = true;
	PP.AmbientOcclusionIntensity = 0.5f;

	PP.bOverride_AmbientOcclusionRadius = true;
	PP.AmbientOcclusionRadius = 60.0f;

	// --- Motion Blur ---
	// Off project-wide: precise platforming reads worse with motion blur,
	// and it would smear the Crush flatten transition.
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.0f;

	// --- Film Grain ---
	// Constant cyanotype/print texture, same in Light World and Crush Mode.
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = 0.03f;

	// --- Lens Flare ---
	// Off: SolarWhite highlights are handled by bloom + emissive, not
	// screen-space flares, so the look stays clean in both modes.
	PP.bOverride_LensFlareIntensity = true;
	PP.LensFlareIntensity = 0.0f;
}