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

    if (bAutoConnectOnBeginPlay)
    {
        Connect();
    }
}

// --- END PLAY ---
void UZenohComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clean shutdown of Zenoh resources
    Disconnect();
    if (Backend)
    {
        delete Backend;
        Backend = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

bool UZenohComponent::Connect()
{
    if (!Backend) return false;

    // 1. Format the Protocol String (tcp or udp)
    FString ProtocolStr = (Protocol == EZenohProtocol::TCP) ? TEXT("tcp") : TEXT("udp");

    // 2. Format the Endpoint String (e.g., "tcp/host.docker.internal:7447")
    FString Endpoint = FString::Printf(TEXT("%s/%s:%d"), *ProtocolStr, *IPAddress, Port);

    // 3. Format the Mode String
    FString ModeStr = (ConnectionMode == EZenohMode::Client) ? TEXT("client") : TEXT("peer");

    UE_LOG(LogTemp, Log, TEXT("[Zenoh] Connecting as %s to %s..."), *ModeStr, *Endpoint);

    // 4. Pass the dynamic settings to the backend
    bool bSuccess = Backend->Initialize(ModeStr, Endpoint);

    if (bSuccess)
    {
        bIsConnected = true; // Track successful connection
        if (!DefaultTopic.IsEmpty())
        {
            Subscribe(DefaultTopic);
        }
    }

    return bSuccess;
}

void UZenohComponent::Disconnect()
{
    if (Backend)
    {
        Backend->Shutdown();
        bIsConnected = false; // Reset tracking state
        UE_LOG(LogTemp, Log, TEXT("[Zenoh] Disconnected."));
    }
}

// --- TICK ---
void UZenohComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// --- PUBLISH ---
bool UZenohComponent::Publish(FString Topic, FString Message)
{
    if (!Backend || !bIsConnected) 
    {
        UE_LOG(LogTemp, Warning, TEXT("[Zenoh] Ignored Publish: Not connected to Zenoh. Call Connect() first!"));
        return false;
    }
    
    return Backend->Publish(Topic, Message);
}

// --- SUBSCRIBE ---
bool UZenohComponent::Subscribe(FString NewTopic)
{
    if (!Backend || !bIsConnected)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Zenoh] Ignored Subscribe: Not connected to Zenoh. Call Connect() first!"));
        return;
    }
    
    if (Backend)
    {
        // A weak pointer safely becomes null if the component is destroyed.
        TWeakObjectPtr<UZenohComponent> WeakThis(this);

        Backend->Subscribe(NewTopic, [WeakThis](const FString& Topic, const FString& Msg)
        {
            // Before handling the message, check if the component is still alive!
            if (UZenohComponent* StrongThis = WeakThis.Get())
            {
                StrongThis->HandleZenohMessage(Topic, Msg);
            }
        });
    }
}

void UZenohComponent::HandleZenohMessage(const FString& Topic, const FString& Payload)
{
    // The component's ONLY job is to take the bytes from the network
    // and hand them to the Blueprint system. Nothing else.
    if (OnMessageReceived.IsBound())
    {
        OnMessageReceived.Broadcast(Topic, Payload);
    }
}