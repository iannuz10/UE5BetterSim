#include "ZenohComponent.h"
#include "ZenohBackend.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"

// --- CONSTRUCTOR ---
UZenohComponent::UZenohComponent()
{
    // Enable ticking if you need it, otherwise false is fine
    PrimaryComponentTick.bCanEverTick = true;

    // CRITICAL: We initialize to nullptr here.
    // We do NOT allocate FZenohBackend yet to prevent loading the DLL at editor startup.
    Backend = nullptr;
}

// --- DESTRUCTOR ---
UZenohComponent::~UZenohComponent()
{
    if (Backend)
    {
        delete Backend;
        Backend = nullptr;
    }
}

// --- BEGIN PLAY ---
void UZenohComponent::BeginPlay()
{
    Super::BeginPlay();

    // Lazy Allocation: Create the backend only when the game actually starts
    if (Backend == nullptr)
    {
        Backend = new FZenohBackend();
    }

    // Initialize and Subscribe
    if (Backend && Backend->Initialize())
    {
        UE_LOG(LogTemp, Log, TEXT("Zenoh Backend Initialized successfully!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Zenoh Backend Failed to Initialize!"));
    }
}

// --- END PLAY ---
void UZenohComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clean shutdown of Zenoh resources
    if (Backend)
    {
        Backend->Shutdown();
    }

    Super::EndPlay(EndPlayReason);
}

// --- TICK ---
void UZenohComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// --- PUBLISH ---
bool UZenohComponent::Publish(FString Topic, FString Message)
{
    if (Backend)
    {
        return Backend->Publish(Topic, Message);
    }
	
    UE_LOG(LogTemp, Warning, TEXT("Zenoh: Cannot publish, backend is null."));
    return false;
}

void UZenohComponent::Subscribe(FString NewTopic)
{
    if (Backend)
    {
        // Instead of broadcasting directly, call HandleZenohMessage
        Backend->Subscribe(NewTopic, [this](const FString& Msg)
        {
            this->HandleZenohMessage(Msg);
        });
    }
}

void UZenohComponent::HandleZenohMessage(const FString& Payload)
{
    // UE_LOG(LogTemp, Warning, TEXT("[Zenoh DEBUG] Raw Payload: %s"), *Payload);

    // This allows Blueprints to handle simple commands like "PING"
    if (OnMessageReceived.IsBound())
    {
        OnMessageReceived.Broadcast(Payload);
    }

    // Try to parse as JSON
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        // UE_LOG(LogTemp, Warning, TEXT("[Zenoh ERROR] JSON Failed to Deserialize! Is the format correct?"));
        return;
    }

    const TSharedPtr<FJsonObject>* LocationObj;
    if (!JsonObject->TryGetObjectField(TEXT("location"), LocationObj))
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh ERROR] Could not find field 'location' in JSON."));
        return;
    }

    // Look for Coordinates
    double X = 0, Y = 0, Z = 0;
    bool bHasX = (*LocationObj)->TryGetNumberField(TEXT("x"), X);
    bool bHasY = (*LocationObj)->TryGetNumberField(TEXT("y"), Y);
    bool bHasZ = (*LocationObj)->TryGetNumberField(TEXT("z"), Z);

    if (bHasX && bHasY && bHasZ)
    {
        FVector TargetLocation(X, Y, Z);
        
        // Success
        // UE_LOG(LogTemp, Log, TEXT("[Zenoh SUCCESS] X: %f, Y: %f, Z: %f"), X, Y, Z);
        OnPositionReceived.Broadcast(TargetLocation);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh ERROR] Missing Coordinate! HasX: %d, HasY: %d, HasZ: %d"), bHasX, bHasY, bHasZ);
    }
}