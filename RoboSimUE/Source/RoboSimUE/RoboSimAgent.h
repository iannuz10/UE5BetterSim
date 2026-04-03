// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoboSimAgent.generated.h"

/**
 * Generic base class for all robot agents in the RoboSim digital twin.
 *
 * Derive every robot Blueprint from this class instead of AActor.
 * This eliminates per-robot Blueprint joint logic and provides a single,
 * optimized C++ path for state application.
 *
 * Usage (Blueprint BeginPlay):
 *   1. Call CacheJointComponents() once after all components are ready.
 *   2. Each frame, call ApplyJointStates(payload) when Zenoh delivers
 *      a sim/agent/{name}/state message.
 */
UCLASS(Blueprintable, BlueprintType)
class ROBOSIMUE_API ARoboSimAgent : public AActor
{
	GENERATED_BODY()

public:
	ARoboSimAgent();

	// ─── Blueprint-callable API ────────────────────────────────────────────

	/**
	 * Call ONCE from Blueprint BeginPlay after all components are initialized.
	 * Iterates all descendant USceneComponents, strips known import-pollution
	 * suffixes (e.g. _DefaultSceneRoot), and indexes them in JointComponentCache
	 * by clean name for O(1) per-frame lookup.
	 */
	UFUNCTION(BlueprintCallable, Category = "RoboSim|Agent")
	void CacheJointComponents();

	/**
	 * Call per-frame from Blueprint when Zenoh delivers a sim/agent/{name}/state message.
	 * Parses hinge and slide joints from the JSON payload and applies:
	 *   - Hinge joints: relative Yaw rotation (radians → degrees, local Z axis).
	 *   - Slide joints: relative Z translation (meters → centimeters, local Z axis).
	 * Assumes the GLB was imported with the robot in its zero/home configuration.
	 */
	UFUNCTION(BlueprintCallable, Category = "RoboSim|Agent")
	void ApplyJointStates(const FString& JsonPayload);

protected:
	/**
	 * Maps cleaned component name → USceneComponent pointer.
	 * Built once by CacheJointComponents(); read-only from Blueprint subclasses.
	 *
	 * The key is the component name with known import-artifact suffixes removed.
	 * MuJoCo joint names arriving in payloads are stripped of their agent-name
	 * prefix (GetName() + "_") before lookup.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RoboSim|Agent")
	TMap<FString, USceneComponent*> JointComponentCache;

	/**
	 * Stores each cached component's relative rotation at the moment CacheJointComponents
	 * ran — i.e. the GLB-imported zero/rest pose.
	 *
	 * Joint angles from MuJoCo are applied as a Yaw DELTA on top of this rest rotation,
	 * preserving any Pitch and Roll baked in by the Blender/LinkForge import pipeline.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RoboSim|Agent")
	TMap<FString, FRotator> JointRestRotations;

private:
	bool bCacheBuilt = false;

	// Anti-spam registry: prevents flooding the log with repeated WARN entries
	// for the same missing joint name during the 60Hz apply loop.
	TSet<FString> WarnedMissingJoints;

	// Anti-spam flags.
	bool bParseErrorLogged = false;

	// Triggers a full diagnostic dump on the first ApplyJointStates call:
	// logs all received joint names, lookup keys, and cache hit/miss result.
	// Reset to false by CacheJointComponents so re-caching re-triggers the dump.
	bool bFirstCallLogged = false;

	/**
	 * Removes well-known UE5 GLB-import artifact suffixes from a component name.
	 * Currently strips: "_DefaultSceneRoot"
	 * Extend this list as additional import pipeline artifacts are discovered.
	 */
	static FString CleanComponentName(const FString& RawName);
};
