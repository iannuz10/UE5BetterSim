#include "RoboSimAgent.h"

ARoboSimAgent::ARoboSimAgent()
{
    PrimaryActorTick.bCanEverTick = true;
    bIsMobile = false;
    RootFrameComponent = nullptr;
}

void ARoboSimAgent::BeginPlay()
{
    Super::BeginPlay();
}

void ARoboSimAgent::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ARoboSimAgent::CacheJointComponents(const TArray<FString>& JointNames, bool bInIsMobile, USceneComponent* InRootFrameComponent)
{
    bIsMobile = bInIsMobile;
    RootFrameComponent = InRootFrameComponent;
    JointsCache.Empty();

    TArray<USceneComponent*> AllComponents;
    GetComponents<USceneComponent>(AllComponents);

    for (const FString& JointName : JointNames)
    {
        FString TargetName = JointName + TEXT("_DefaultSceneRoot");
        USceneComponent* FoundComponent = nullptr;

        for (USceneComponent* Comp : AllComponents)
        {
            if (Comp->GetName() == TargetName)
            {
                FoundComponent = Comp;
                break;
            }
        }

        if (FoundComponent)
        {
            JointsCache.Add(JointName, FoundComponent);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ARoboSimAgent: Joint component not found for %s (Expected name: %s)"), *JointName, *TargetName);
        }
    }
}

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void ARoboSimAgent::ApplyUnifiedState(const FString& JsonPayload)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("ARoboSimAgent: Failed to parse JSON payload."));
        return;
    }

    // 1. Root Transform (Mobile Base)
    if (bIsMobile && RootFrameComponent && JsonObject->HasField(TEXT("root_transform")))
    {
        const TSharedPtr<FJsonObject> RootObj = JsonObject->GetObjectField(TEXT("root_transform"));
        
        if (RootObj->HasField(TEXT("pos")) && RootObj->HasField(TEXT("quat")))
        {
            const TArray<TSharedPtr<FJsonValue>> PosArr = RootObj->GetArrayField(TEXT("pos"));
            const TArray<TSharedPtr<FJsonValue>> QuatArr = RootObj->GetArrayField(TEXT("quat"));

            if (PosArr.Num() == 3 && QuatArr.Num() == 4)
            {
                FVector GlobalPos(PosArr[0]->AsNumber(), PosArr[1]->AsNumber(), PosArr[2]->AsNumber());
                FQuat GlobalQuat(QuatArr[0]->AsNumber(), QuatArr[1]->AsNumber(), QuatArr[2]->AsNumber(), QuatArr[3]->AsNumber());
                
                RootFrameComponent->SetWorldLocationAndRotation(GlobalPos, GlobalQuat);
            }
        }
    }

    // 2. Joints
    if (JsonObject->HasField(TEXT("joints")))
    {
        const TSharedPtr<FJsonObject> JointsObj = JsonObject->GetObjectField(TEXT("joints"));

        // Hinge joints
        if (JointsObj->HasField(TEXT("hinge")))
        {
            const TSharedPtr<FJsonObject> HingeObj = JointsObj->GetObjectField(TEXT("hinge"));
            for (auto& Pair : HingeObj->Values)
            {
                USceneComponent** CompPtr = JointsCache.Find(Pair.Key);
                if (CompPtr && *CompPtr)
                {
                    float ValueRad = Pair.Value->AsNumber();
                    float ValueDeg = FMath::RadiansToDegrees(ValueRad);
                    
                    // Apply rotation. Assuming Z-axis is the hinge axis (standard for UE5 imported robots).
                    (*CompPtr)->SetRelativeRotation(FRotator(0.0f, ValueDeg, 0.0f));
                }
            }
        }

        // Slide joints
        if (JointsObj->HasField(TEXT("slide")))
        {
            const TSharedPtr<FJsonObject> SlideObj = JointsObj->GetObjectField(TEXT("slide"));
            for (auto& Pair : SlideObj->Values)
            {
                USceneComponent** CompPtr = JointsCache.Find(Pair.Key);
                if (CompPtr && *CompPtr)
                {
                    float ValueMeters = Pair.Value->AsNumber();
                    float ValueCm = ValueMeters * 100.0f;
                    
                    // Apply translation. Assuming Z-axis is the slide axis.
                    (*CompPtr)->SetRelativeLocation(FVector(0.0f, 0.0f, ValueCm));
                }
            }
        }
    }
}
