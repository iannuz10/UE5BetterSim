// Fill out your copyright notice in the Description page of Project Settings.


#include "SceneExporter.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "RoboSimAgent.h"

// Define the static spam filter variable
TSet<FString> USceneExporter::AlreadyWarnedActors;

// Define the static actor cache
TMap<FString, TWeakObjectPtr<AActor>> USceneExporter::CachedSimulatedActors;

// ==========================================
// TELEMETRY IMPLEMENTATION
// ==========================================

void USceneExporter::ClearSimulationLog()
{
	FString TelemetryLogPath = FPaths::ProjectSavedDir() + TEXT("Logs/RoboSim_Telemetry.log");
	FString InitText = FString::Printf(TEXT("=== RoboSim Telemetry Log Initialized at %s ===\n"), *FDateTime::Now().ToString());
    
	// Default SaveStringToFile overwrites the file. Clean slate for the new run.
	FFileHelper::SaveStringToFile(InitText, *TelemetryLogPath);
	AlreadyWarnedActors.Empty();
}

void USceneExporter::WriteSimulationLog(const FString& Level, const FString& Message)
{
	FString TelemetryLogPath = FPaths::ProjectSavedDir() + TEXT("Logs/RoboSim_Telemetry.log");
	FString Timestamp = FDateTime::Now().ToString(TEXT("%H:%M:%S"));
	FString LogEntry = FString::Printf(TEXT("[%s] [%s] %s\n"), *Timestamp, *Level, *Message);
    
	// UE5 requires explicit Encoding and FileManager arguments before the WriteFlag.
	FFileHelper::SaveStringToFile(
		LogEntry, 
		*TelemetryLogPath, 
		FFileHelper::EEncodingOptions::AutoDetect, 
		&IFileManager::Get(), 
		FILEWRITE_Append
	);

	// Mirror to standard Output Log
	if (Level == TEXT("ERROR") || Level == TEXT("FATAL")) {
		UE_LOG(LogTemp, Error, TEXT("%s"), *LogEntry);
	} else if (Level == TEXT("WARN")) {
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LogEntry);
	} else {
		UE_LOG(LogTemp, Log, TEXT("%s"), *LogEntry);
	}
}

// ==========================================
// CORE LOGIC
// ==========================================

FString USceneExporter::GenerateWorldJSON(const UObject* WorldContextObject, const TArray<FName>& ActorTags)
{
	
	WriteSimulationLog(TEXT("INFO"), TEXT("--- STARTING UE5 WORLD EXPORT ---"));
	
	// Init root JSON object 
	TSharedPtr<FJsonObject> RootJson = MakeShareable(new FJsonObject());
	TArray<TSharedPtr<FJsonValue>> ObjectsArray;
	
	// Find all actors with tag
	TArray<AActor*> FoundActors;
	for (const FName ActorTag : ActorTags)
	{
		TArray<AActor*> TempActors;
		UGameplayStatics::GetAllActorsWithTag(WorldContextObject, ActorTag, TempActors);
		for (AActor* Actor : TempActors)
		{
			if (Actor)
			{
				FoundActors.AddUnique(Actor);
			}
		}
	}
	
	WriteSimulationLog(TEXT("INFO"), FString::Printf(TEXT("Discovered %d actors matching export tags."), FoundActors.Num()));
	
	for (AActor* Actor : FoundActors)
	{
		FString ActorID;

		// 1. Cast the Actor to see if it's one of our Agents
		if (ARoboSimAgent* Agent = Cast<ARoboSimAgent>(Actor))
		{
			// It's a robot! Use the clean, deterministic ID.
			ActorID = Agent->GetRegisteredID();
		}
		else
		{
			// It's a static mesh, cube, or plane. Use the default UE name.
			ActorID = Actor->GetName();
		}

		FString ActorNameLower = ActorID.ToLower();

		// ------------------------------------------------------------------
		// STRICT VALIDATION: File Tags
		// ------------------------------------------------------------------
		FString FileName = TEXT(""); 
		for (FName Tag : Actor->Tags)
		{
			FString TagStr = Tag.ToString();
			if (TagStr.StartsWith(TEXT("File:")))
			{
				TagStr.Split(TEXT(":"), nullptr, &FileName);
				break;
			}
		}

		// ------------------------------------------------------------------
		// STRICT VALIDATION: Mesh Types
		// If we don't know what it is, we abort this actor.
		// ------------------------------------------------------------------
		FString MeshType = TEXT("");
		TSharedPtr<FJsonObject> ObjJson = MakeShareable(new FJsonObject());
		ObjJson->SetStringField(TEXT("name"), ActorID);

		// Determine mesh type based on tags
		if (Actor->ActorHasTag(FName("Agent")))
		{
			if (FileName.IsEmpty())
			{
				WriteSimulationLog(TEXT("FATAL"), FString::Printf(TEXT("Agent '%s' is missing a 'File:XYZ' tag. Cannot export robot. Skipping."), *ActorID));
				continue; // Bail out.
			}
			MeshType = TEXT("agent");
			ObjJson->SetStringField(TEXT("file_path"), FString::Printf(TEXT("assets/robots/%s.xml"), *FileName));
            
			// Force explicit Mobile tag definition
			if (!Actor->ActorHasTag(FName("Mobile")) && !Actor->ActorHasTag(FName("Bolted"))) {
				WriteSimulationLog(TEXT("FATAL"), FString::Printf(TEXT("Agent '%s' lacks 'Mobile' or 'Bolted' tag. Physics will crash. Skipping."), *ActorID));
				continue;
			}
			ObjJson->SetBoolField(TEXT("is_mobile"), Actor->ActorHasTag(FName("Mobile")));
		}
		else if (Actor->ActorHasTag(FName("Custom")))
		{
			if (FileName.IsEmpty())
			{
				WriteSimulationLog(TEXT("FATAL"), FString::Printf(TEXT("Custom mesh '%s' is missing a 'File:XYZ' tag. Skipping."), *ActorID));
				continue; 
			}
			MeshType = TEXT("custom");
			ObjJson->SetStringField(TEXT("file_path"), FString::Printf(TEXT("assets/environment/%s.obj"), *FileName));
		}
		else if (Actor->ActorHasTag(FName("Sphere")) || ActorNameLower.Contains(TEXT("sphere")))
		{
			MeshType = TEXT("sphere");
		}
		else if (Actor->ActorHasTag(FName("Cube")) || ActorNameLower.Contains(TEXT("cube")))
		{
			MeshType = TEXT("cube");
		}
		else if (Actor->ActorHasTag(FName("Plane")) || Actor->ActorHasTag(FName("Floor")) || ActorNameLower.Contains(TEXT("floor")) || ActorNameLower.Contains(TEXT("plane")))
		{
			MeshType = TEXT("plane");
		}
		else 
		{
			// We have absolutely no idea what this is. Don't guess.
			WriteSimulationLog(TEXT("FATAL"), FString::Printf(TEXT("Actor '%s' has no recognizable type tags (Agent/Custom/Cube/etc). Skipping."), *ActorID));
			continue; 
		}

		ObjJson->SetStringField(TEXT("mesh"), MeshType);
		
		bool bIsStatic = Actor->ActorHasTag(FName("StaticProp"));
		ObjJson->SetBoolField(TEXT("is_static"), bIsStatic);

		// ==========================================
		// MATH TRANSFORMS (MuJoCo format)
		// Coordinate convention (UE5 to scientific)
		// UE5 left-handed (X forward, Y right, Z up)
		// Scientific right-handed (X forward, Y left, Z up)
		// ==========================================
		FVector Pos = Actor->GetActorLocation() / 100.0; // cm to meters
		Pos.Y = -Pos.Y; // Left to Right-handed conversion

		FQuat Quat = Actor->GetActorQuat();
		FQuat SciRot(-Quat.X, Quat.Y, -Quat.Z, Quat.W); // Invert X and Z for right-handed

		TArray<TSharedPtr<FJsonValue>> PosArray;
		// Convert cm to m
		PosArray.Add(MakeShareable(new FJsonValueNumber(Pos.X)));
		PosArray.Add(MakeShareable(new FJsonValueNumber(Pos.Y)));
		PosArray.Add(MakeShareable(new FJsonValueNumber(Pos.Z)));
		ObjJson->SetArrayField(TEXT("position"), PosArray);
		
		TArray<TSharedPtr<FJsonValue>> QuatArray;
		QuatArray.Add(MakeShareable(new FJsonValueNumber(SciRot.W)));
		QuatArray.Add(MakeShareable(new FJsonValueNumber(SciRot.X)));
		QuatArray.Add(MakeShareable(new FJsonValueNumber(SciRot.Y)));
		QuatArray.Add(MakeShareable(new FJsonValueNumber(SciRot.Z)));
		ObjJson->SetArrayField(TEXT("quat"), QuatArray);

		// ------------------------------------------------------------------
		// STRICT VALIDATION: Physical Bounds
		// ------------------------------------------------------------------
		UStaticMeshComponent* MeshComp = Actor->FindComponentByClass<UStaticMeshComponent>();

		if (!MeshComp || !MeshComp->GetStaticMesh())
		{
			// Planes and pure Blueprint Agents might not have a static mesh root, which is fine.
			// But if it's a cube, sphere, or custom prop, we NEED the mesh component to calculate physical size.
			if (MeshType == TEXT("custom") || MeshType == TEXT("cube") || MeshType == TEXT("sphere")) 
			{
				WriteSimulationLog(TEXT("FATAL"), FString::Printf(TEXT("Actor '%s' (%s) lacks a valid UStaticMeshComponent. Cannot compute physics bounds. Skipping."), *ActorID, *MeshType));
				continue;
			}
		}
		else
		{
			// Get the raw, unrotated 3D model's bounding box
			FBox LocalBox = MeshComp->GetStaticMesh()->GetBoundingBox();
            
			// Get the Extent (which is already the Half-Size in Centimeters)
			FVector LocalExtent = LocalBox.GetExtent(); 
            
			// Get the Actor's Scale Multiplier
			FVector ActorScale = Actor->GetActorScale3D();
            
			// Calculate the TRUE physical half-size in cm (Immune to rotation)
			FVector TrueExtent = LocalExtent * ActorScale;
            
			// Convert to MuJoCo format (Centimeters to Meters)
			TArray<TSharedPtr<FJsonValue>> SizeArray;
			SizeArray.Add(MakeShareable(new FJsonValueNumber(TrueExtent.X / 100.0))); 
			SizeArray.Add(MakeShareable(new FJsonValueNumber(TrueExtent.Y / 100.0))); 
			SizeArray.Add(MakeShareable(new FJsonValueNumber(TrueExtent.Z / 100.0)));
			ObjJson->SetArrayField(TEXT("size"), SizeArray);

			TArray<TSharedPtr<FJsonValue>> ScaleArray;
			ScaleArray.Add(MakeShareable(new FJsonValueNumber(ActorScale.X)));
			ScaleArray.Add(MakeShareable(new FJsonValueNumber(ActorScale.Y)));
			ScaleArray.Add(MakeShareable(new FJsonValueNumber(ActorScale.Z)));
			ObjJson->SetArrayField(TEXT("scale"), ScaleArray);
		}
		
		// Only add to the final array if it survived all the gauntlets above.
		ObjectsArray.Add(MakeShareable(new FJsonValueObject(ObjJson)));
		
		WriteSimulationLog(TEXT("DEBUG"), FString::Printf(TEXT("Exported Object: %s | Mesh: %s | Static: %s | Pos: [%.2f, %.2f, %.2f]"), 
			*ActorID, *MeshType, bIsStatic ? TEXT("True") : TEXT("False"), Pos.X, Pos.Y, Pos.Z));
	}

	RootJson->SetArrayField(TEXT("objects"), ObjectsArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootJson.ToSharedRef(), Writer);

	WriteSimulationLog(TEXT("INFO"), TEXT("--- EXPORT COMPLETE ---"));
	
	return OutputString;
}

// ==========================================
// IMPORT LOGIC (Runs at 60Hz - Beware File I/O!)
// ==========================================

void USceneExporter::ApplyWorldStateJSON(const UObject* WorldContextObject, const TArray<FName>& ActorTags, TSharedPtr<FJsonObject> JsonObject)
{
	if (!JsonObject.IsValid())
	{
		if (!AlreadyWarnedActors.Contains(TEXT("ApplyWorldState_JSON_FAIL")))
		{
			WriteSimulationLog(TEXT("ERROR"), TEXT("ApplyWorldState: Received invalid JSON object."));
			AlreadyWarnedActors.Add(TEXT("ApplyWorldState_JSON_FAIL"));
		}
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* ObjectsArray;
	if (!JsonObject->TryGetArrayField(TEXT("objects"), ObjectsArray)) return;

	// O(1) PERFORMANCE OPTIMIZATION: 
	// Instead of UGameplayStatics::GetAllActorsWithTag at 60Hz, we use our pre-built cache.
	
	// If the cache is empty, we perform a late initialization as a fallback, 
	// though the Simulation Manager should ideally call BuildActorCache on BeginPlay.
	if (CachedSimulatedActors.Num() == 0)
	{
		BuildActorCache(WorldContextObject, ActorTags);
	}

	for (TSharedPtr<FJsonValue> ObjVal : *ObjectsArray)
	{
		TSharedPtr<FJsonObject> ObjMap = ObjVal->AsObject();
		if (!ObjMap.IsValid()) continue;

		FString ActorName = ObjMap->GetStringField(TEXT("name"));
		
		// O(1) Lookup
		if (TWeakObjectPtr<AActor>* WeakActorPtr = CachedSimulatedActors.Find(ActorName))
		{
			if (WeakActorPtr->IsValid())
			{
				ApplyTransformToActor(WeakActorPtr->Get(), ObjMap);
			}
			else 
			{
				// Stale reference - remove from cache to prevent further lookups
				CachedSimulatedActors.Remove(ActorName);
				WriteSimulationLog(TEXT("WARN"), FString::Printf(TEXT("Actor '%s' was in cache but is no longer valid. Removed."), *ActorName));
			}
		}
		else if (!AlreadyWarnedActors.Contains(ActorName))
		{
			WriteSimulationLog(TEXT("WARN"), FString::Printf(TEXT("Received coordinates for '%s', but no actor found in cache."), *ActorName));
			AlreadyWarnedActors.Add(ActorName);
		}
	}
}

void USceneExporter::BuildActorCache(const UObject* WorldContextObject, const TArray<FName>& ActorTags)
{
	WriteSimulationLog(TEXT("INFO"), TEXT("Building Simulation Actor Cache..."));
	
	CachedSimulatedActors.Empty();

	TArray<AActor*> FoundActors;
	for (const FName& Tag : ActorTags)
	{
		TArray<AActor*> TempActors;
		UGameplayStatics::GetAllActorsWithTag(WorldContextObject, Tag, TempActors);
		for (AActor* Actor : TempActors)
		{
			if (Actor)
			{
				FoundActors.AddUnique(Actor);
			}
		}
	}

	for (AActor* Actor : FoundActors)
	{
		// 1. Cast the Actor to see if it's one of our Agents
		if (ARoboSimAgent* Agent = Cast<ARoboSimAgent>(Actor))
		{
			// Match the JSON! Use the deterministic ID as the cache key.
			CachedSimulatedActors.Add(Agent->GetRegisteredID(), Actor);
		}
		else
		{
			// Fallback for static props
			CachedSimulatedActors.Add(Actor->GetName(), Actor);
		}
	}

	WriteSimulationLog(TEXT("INFO"), FString::Printf(TEXT("Cache built successfully. Monitoring %d actors."), CachedSimulatedActors.Num()));
}

void USceneExporter::ClearActorCache()
{
	WriteSimulationLog(TEXT("INFO"), TEXT("Clearing Simulation Actor Cache."));
	CachedSimulatedActors.Empty();
}

void USceneExporter::ApplyTransformToActor(AActor* Actor, const TSharedPtr<FJsonObject>& ObjMap)
{
	if (!Actor || !ObjMap.IsValid()) return;

	const TArray<TSharedPtr<FJsonValue>>* PosArray;
	const TArray<TSharedPtr<FJsonValue>>* QuatArray;

	if (ObjMap->TryGetArrayField(TEXT("pos"), PosArray) && PosArray->Num() == 3 &&
		ObjMap->TryGetArrayField(TEXT("quat"), QuatArray) && QuatArray->Num() == 4)
	{
		FVector NewPos(
			(*PosArray)[0]->AsNumber(), 
			(*PosArray)[1]->AsNumber(), 
			(*PosArray)[2]->AsNumber()
		);
		
		FQuat NewQuat(
			(*QuatArray)[0]->AsNumber(), 
			(*QuatArray)[1]->AsNumber(), 
			(*QuatArray)[2]->AsNumber(), 
			(*QuatArray)[3]->AsNumber()
		);
		
		NewQuat.Normalize();
		Actor->SetActorLocationAndRotation(NewPos, NewQuat, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

// ==========================================
// LATENCY PROBE
// ==========================================

FString USceneExporter::GetMsgIdFromPayload(const FString& Payload)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return FString();
	}
	if (!JsonObject->HasField(TEXT("_msg_id")))
	{
		return FString();
	}
	int64 MsgId = (int64)JsonObject->GetNumberField(TEXT("_msg_id"));
	return FString::Printf(TEXT("%lld"), MsgId);
}

bool USceneExporter::ParseJointData(const FString& JsonString, TMap<FString, float>& OutHingeJoints, TMap<FString, float>& OutSlideJoints)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()) return false;

	const TSharedPtr<FJsonObject>* JointsObj;
	if (JsonObject->TryGetObjectField(TEXT("joints"), JointsObj))
	{
		const TSharedPtr<FJsonObject>* HingeObj;
		if ((*JointsObj)->TryGetObjectField(TEXT("hinge"), HingeObj))
		{
			for (auto& Pair : (*HingeObj)->Values) OutHingeJoints.Add(Pair.Key, Pair.Value->AsNumber());
		}

		const TSharedPtr<FJsonObject>* SlideObj;
		if ((*JointsObj)->TryGetObjectField(TEXT("slide"), SlideObj))
		{
			for (auto& Pair : (*SlideObj)->Values) OutSlideJoints.Add(Pair.Key, Pair.Value->AsNumber());
		}
		return true;
	}
	return false;
}