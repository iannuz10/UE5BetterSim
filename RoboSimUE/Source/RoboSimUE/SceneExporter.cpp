// Fill out your copyright notice in the Description page of Project Settings.


#include "SceneExporter.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FString USceneExporter::GenerateWorldJSON(const UObject* WorldContextObject, FName ActorTag)
{
	// Find all actors with tag
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(WorldContextObject, ActorTag, FoundActors);

	TSharedPtr<FJsonObject> RootJson = MakeShareable(new FJsonObject());
	TArray<TSharedPtr<FJsonValue>> JsonObjectArray;

	for (AActor* Actor : FoundActors)
	{
		if (!Actor) continue;

		TSharedPtr<FJsonObject> ActorJson = MakeShareable(new FJsonObject());

		ActorJson->SetStringField("name", Actor->GetName());

		// Coordinate convention (UE5 to scientific)
		// UE5 left-handed (X forward, Y right, Z up)
		// Scientific right-handed (X forward, Y left, Z up)
		FVector UELoc = Actor->GetActorLocation();

		TArray<TSharedPtr<FJsonValue>> PosArray;
		// Convert cm to m
		PosArray.Add(MakeShareable(new FJsonValueNumber(UELoc.X / 100.0))); 
		PosArray.Add(MakeShareable(new FJsonValueNumber(-UELoc.Y / 100.0))); // Invert Y for right-handed
		PosArray.Add(MakeShareable(new FJsonValueNumber(UELoc.Z / 100.0))); 
		ActorJson->SetArrayField("position", PosArray);

		// Add more fields as needed (rotation as quaternion)
		FQuat UERot = Actor->GetActorQuat();
		FQuat SciRot(-UERot.X, UERot.Y, -UERot.Z, UERot.W); // Invert X and Z for right-handed
		
		TArray<TSharedPtr<FJsonValue>> QuatArray;
		QuatArray.Add(MakeShareable(new FJsonValueNumber(SciRot.W)));
		QuatArray.Add(MakeShareable(new FJsonValueNumber(SciRot.X)));
		QuatArray.Add(MakeShareable(new FJsonValueNumber(SciRot.Y)));
		QuatArray.Add(MakeShareable(new FJsonValueNumber(SciRot.Z)));
		ActorJson->SetArrayField("quat", QuatArray);

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
            
			ActorJson->SetArrayField("size", SizeArray);

			// get shape info from StaticMeshComponent if available
			FString MeshName = MeshComp->GetStaticMesh()->GetName();
			ActorJson->SetStringField("mesh", MeshName);
		}
		
		JsonObjectArray.Add(MakeShareable(new FJsonValueObject(ActorJson)));
	}

	RootJson->SetArrayField("objects", JsonObjectArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootJson.ToSharedRef(), Writer);

	return OutputString;
}

void USceneExporter::ApplyWorldStateJSON(const UObject* WorldContextObject, FName ActorTag, const FString& JsonString)
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
	if (!JsonObject->TryGetArrayField("objects", ObjectsArray)){
		UE_LOG(LogTemp, Warning, TEXT("JSON does not contain 'objects' array!"));
		return;
	}

	// find all actors with received tag
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(WorldContextObject, ActorTag, FoundActors);

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