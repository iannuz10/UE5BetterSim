// Fill out your copyright notice in the Description page of Project Settings.


#include "SceneExporter.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FString USceneExporter::GenerateWorldJSON(const UObject* WorldContextObject, const TArray<FName>& ActorTags)
{
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
	
	for (AActor* Actor : FoundActors)
	{
		TSharedPtr<FJsonObject> ObjJson = MakeShareable(new FJsonObject());

		FString ActorID = Actor->GetName();
		ObjJson->SetStringField(TEXT("name"), ActorID);

		FString MeshType = TEXT("cube"); // Default fallback
		FString FileName = ActorID; // Default fallback

		// Check tags for mesh type and file name
		for (FName Tag : Actor->Tags)
		{
			FString TagStr = Tag.ToString();
			if (TagStr.StartsWith(TEXT("File:")))
			{
				TagStr.Split(TEXT(":"), nullptr, &FileName);
				break;
			}
		}

		FString ActorNameLower = ActorID.ToLower();

		// Determine mesh type based on tags
		if (Actor->ActorHasTag(FName("Agent")))
		{
			MeshType = TEXT("agent");
			ObjJson->SetStringField(TEXT("file_path"), FString::Printf(TEXT("assets/robots/%s/model.urdf"), *FileName));
            
			// Tell MuJoCo if this is a flying/walking robot or a bolted robotic arm!
			bool bIsMobile = Actor->ActorHasTag(FName("Mobile"));
			ObjJson->SetBoolField(TEXT("is_mobile"), bIsMobile);
		}
		else if (Actor->ActorHasTag(FName("Custom")))
		{
			MeshType = TEXT("custom");
			ObjJson->SetStringField("file_path", FString::Printf(TEXT("assets/environment/%s.obj"), *FileName));
		}
		else if (Actor->ActorHasTag(FName("Sphere")) || ActorNameLower.Contains(TEXT("sphere")))
		{
			MeshType = TEXT("sphere");
		}
		// NEW: Smart detection! Catch floors and planes automatically to prevent the void-falling bug!
		else if (Actor->ActorHasTag(FName("Plane")) || Actor->ActorHasTag(FName("Floor")) || ActorNameLower.Contains(TEXT("floor")) || ActorNameLower.Contains(TEXT("plane")))
		{
			MeshType = TEXT("plane");
		}

		ObjJson->SetStringField(TEXT("mesh"), MeshType);

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

		UStaticMeshComponent* MeshComp = Actor->FindComponentByClass<UStaticMeshComponent>();
		if (MeshComp && MeshComp->GetStaticMesh())
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
		}
		ObjectsArray.Add(MakeShareable(new FJsonValueObject(ObjJson)));
	}

	RootJson->SetArrayField(TEXT("objects"), ObjectsArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootJson.ToSharedRef(), Writer);

	return OutputString;
}

void USceneExporter::ApplyWorldStateJSON(const UObject* WorldContextObject, const TArray<FName>& ActorTags, const FString& JsonString)
{
	// Parse JSON String
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to parse JSON!"));
		return;
	}

	// Get objects array
	const TArray<TSharedPtr<FJsonValue>>* ObjectsArray;
	if (!JsonObject->TryGetArrayField(TEXT("objects"), ObjectsArray)){
		UE_LOG(LogTemp, Warning, TEXT("JSON does not contain 'objects' array!"));
		return;
	}

	// find all actors with received tag
	TArray<AActor*> FoundActors;
	for (const FName& Tag : ActorTags)
	{
		TArray<AActor*> TempActors;
		UGameplayStatics::GetAllActorsWithTag(WorldContextObject, Tag, TempActors);
		for (AActor* Actor : TempActors)
		{
			FoundActors.AddUnique(Actor);
		}
	}

	// Loop through JSON objects and apply to actors
	for (TSharedPtr<FJsonValue> ObjVal : *ObjectsArray)
	{
		TSharedPtr<FJsonObject> ObjMap = ObjVal->AsObject();
		if (!ObjMap.IsValid()) continue;

		FString ActorName = ObjMap->GetStringField(TEXT("name"));
		// Find the matching actor in Unreal
		for (AActor* Actor : FoundActors)
		{
			if (Actor->GetName() == ActorName)
			{
				// Apply Location and Rotation
				const TArray<TSharedPtr<FJsonValue>>* PosArray;
				const TArray<TSharedPtr<FJsonValue>>* QuatArray;
				
				if (ObjMap->TryGetArrayField(TEXT("pos"), PosArray) && PosArray->Num() == 3 && ObjMap->TryGetArrayField(TEXT("quat"), QuatArray) && QuatArray->Num() == 4)
				{
					FVector NewPos((*PosArray)[0]->AsNumber(), (*PosArray)[1]->AsNumber(), (*PosArray)[2]->AsNumber());
					// Remember: [X, Y, Z, W] in the Python script to match Unreal
					FQuat NewQuat((*QuatArray)[0]->AsNumber(), (*QuatArray)[1]->AsNumber(), (*QuatArray)[2]->AsNumber(), (*QuatArray)[3]->AsNumber());
					NewQuat.Normalize();
					Actor->SetActorLocationAndRotation(NewPos, NewQuat, false, nullptr, ETeleportType::TeleportPhysics);
				}
                
				break; 
			}
		}
	}
	
}

bool USceneExporter::ParseJointData(const FString& JsonString, TMap<FString, float>& OutHingeJoints, TMap<FString, float>& OutSlideJoints)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()) return false;

	const TSharedPtr<FJsonObject>* JointsObj;
	if (JsonObject->TryGetObjectField(TEXT("joints"), JointsObj))
	{
		// NEW: Python now sends "hinge" and "slide" sub-objects!
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