// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SceneExporter.generated.h"

/**
 * 
 */
UCLASS()
class ROBOSIMUE_API USceneExporter : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ==========================================
	// TELEMETRY & LOGGING
	// ==========================================
    
	// Call this ONCE on BeginPlay in your Simulation Manager to wipe the old log file.
	UFUNCTION(BlueprintCallable, Category = "Simulation|Logging")
	static void ClearSimulationLog();

	// Internal helper to format and write logs to disk and the UE5 console.
	static void WriteSimulationLog(const FString& Level, const FString& Message);
	
	// ==========================================
	// EXPORT / IMPORT
	// ==========================================
	UFUNCTION(BlueprintCallable, Category = "Simulation|Export", meta = (WorldContext = "WorldContextObject"))	
	static FString GenerateWorldJSON(const UObject* WorldContextObject, const TArray<FName>& ActorTags);

	UFUNCTION(BlueprintCallable, Category = "Simulation|Import", meta = (WorldContext = "WorldContextObject"))
	static void ApplyWorldStateJSON(const UObject* WorldContextObject, const TArray<FName>& ActorTags, const FString& JsonString);

	/** 
	 * Build the static actor cache for O(1) lookups during state application.
	 * Should be called once on BeginPlay in the Simulation Manager.
	 */
	UFUNCTION(BlueprintCallable, Category = "Simulation|Optimization", meta = (WorldContext = "WorldContextObject"))
	static void BuildActorCache(const UObject* WorldContextObject, const TArray<FName>& ActorTags);

	/**
	 * Clears the actor cache. Should be called on EndPlay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Simulation|Optimization")
	static void ClearActorCache();

	// Splits the data into two Maps so Blueprints know which math to apply
	UFUNCTION(BlueprintCallable, Category = "Simulation|Import")
	static bool ParseJointData(const FString& JsonString, TMap<FString, float>& OutHingeJoints, TMap<FString, float>& OutSlideJoints);

	// ==========================================
	// LATENCY PROBE
	// ==========================================
	// Extracts the _msg_id field from an agent state payload so Blueprint can publish an ACK.
	// Returns an empty string if the field is absent (e.g., world state messages).
	UFUNCTION(BlueprintCallable, Category = "Simulation|Latency")
	static FString GetMsgIdFromPayload(const FString& Payload);

	private:
	// Internal helper to apply transform data to an actor
	static void ApplyTransformToActor(AActor* Actor, const TSharedPtr<FJsonObject>& ObjMap);

	// Map of Actor Name to a weak pointer of the Actor for O(1) retrieval during 60Hz loop.
	// This is static because USceneExporter is a BlueprintFunctionLibrary.
	static TMap<FString, TWeakObjectPtr<AActor>> CachedSimulatedActors;

	// Anti-spam registry. Prevents the game loop from writing 10,000 lines a second to the hard drive if something breaks.
	static TSet<FString> AlreadyWarnedActors;
	};
