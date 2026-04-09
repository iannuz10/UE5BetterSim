#include "SimWorldManager.h"
#include "SceneExporter.h"
#include "ZenohSubsystem.h"
#include "Engine/GameInstance.h"

ASimWorldManager::ASimWorldManager()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ASimWorldManager::BeginPlay()
{
    Super::BeginPlay();

    // 1. Build the O(1) Actor Cache for high-performance importing
    USceneExporter::BuildActorCache(this, WorldActorTags);

    // 2. Subscribe to the world state topic via Zenoh
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UZenohSubsystem* ZenohSubsystem = GameInstance->GetSubsystem<UZenohSubsystem>())
        {
            UZenohTopicListener* Listener = ZenohSubsystem->SubscribeToTopic(WorldConnectionName, WorldTopic);
            if (Listener)
            {
                ActiveListeners.Add(Listener);
                Listener->OnTopicParsed.AddUObject(this, &ASimWorldManager::HandleWorldStateUpdate);
                USceneExporter::WriteSimulationLog(TEXT("INFO"), FString::Printf(TEXT("ASimWorldManager subscribed to topic: %s"), *WorldTopic));
            }
            else
            {
                USceneExporter::WriteSimulationLog(TEXT("ERROR"), FString::Printf(TEXT("ASimWorldManager failed to subscribe to topic: %s"), *WorldTopic));
            }
        }
    }
}

void ASimWorldManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Cleanly unbind delegates
    for (UZenohTopicListener* Listener : ActiveListeners)
    {
        if (Listener)
        {
            Listener->OnTopicParsed.RemoveAll(this);
        }
    }
    ActiveListeners.Empty();

    // Clear the static cache
    USceneExporter::ClearActorCache();

    Super::EndPlay(EndPlayReason);
}

void ASimWorldManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ASimWorldManager::PublishWorldInit()
{
    FString InitJson = USceneExporter::GenerateWorldJSON(this, WorldActorTags);

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UZenohSubsystem* ZenohSubsystem = GameInstance->GetSubsystem<UZenohSubsystem>())
        {
            if (ZenohSubsystem->Publish(WorldConnectionName, TEXT("sim/world/init"), InitJson))
            {
                USceneExporter::WriteSimulationLog(TEXT("INFO"), TEXT("Published sim/world/init successfully."));
            }
            else
            {
                USceneExporter::WriteSimulationLog(TEXT("ERROR"), TEXT("Failed to publish sim/world/init."));
            }
        }
    }
}

void ASimWorldManager::HandleWorldStateUpdate(TSharedPtr<FJsonObject> JsonObject)
{
    if (JsonObject.IsValid())
    {
        USceneExporter::ApplyWorldStateJSON(this, WorldActorTags, JsonObject);
        ReceiveWorldStateUpdated();
    }
}
