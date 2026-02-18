#include "ZenohComponent.h"
#include "ZenohBackend.h"

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

        // Subscribe to the default topic
        Backend->Subscribe("sim/command", [this](const FString& Msg)
        {
            // The backend ensures this runs on the Game Thread, so it's safe to broadcast
            this->OnMessageReceived.Broadcast(Msg);
        });
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