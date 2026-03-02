#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h" 
#include "ZenohBackend.h"
#include "ZenohSubsystem.generated.h"

// Enum configurations
UENUM(BlueprintType)
enum class EZenohProtocol : uint8
{
    TCP UMETA(DisplayName = "TCP (Reliable, Localhost)"),
    UDP UMETA(DisplayName = "UDP (Fast, Networked)")
};

UENUM(BlueprintType)
enum class EZenohMode : uint8
{
    Client UMETA(DisplayName = "Client (Connects to Router/Peer)"),
    Peer UMETA(DisplayName = "Peer (Direct Mesh Network)")
};

USTRUCT(BlueprintType)
struct FZenohConnectionInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Connection")
    FName ConnectionName = TEXT("DefaultConnection");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Connection")
    EZenohMode ConnectionMode = EZenohMode::Client;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Connection")
    EZenohProtocol Protocol = EZenohProtocol::TCP;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Connection")
    FString IPAddress = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Connection")
    int32 Port = 7447;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnZenohTopicMessage, const FString&, Message);

UCLASS(BlueprintType)
class ZENOHBRIDGE_API UZenohTopicListener : public UObject
{
    GENERATED_BODY()
public:
    // It only outputs the message, because the Topic and Connection are already known
    UPROPERTY(BlueprintAssignable, Category = "Zenoh|Messaging")
    FOnZenohTopicMessage OnMessageReceived;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnZenohGlobalMessage, const FName&, ConnectionName, const FString&, Topic, const FString&, Message);

UCLASS()
class ZENOHBRIDGE_API UZenohSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    // ==========================================
    // SUBSYSTEM LIFECYCLE 
    // ==========================================
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ==========================================
    // FTICKABLE GAMEOBJECT INTERFACE
    // ==========================================
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override { return !HasAnyFlags(RF_ClassDefaultObject); }

    // ==========================================
    // LIFECYCLE CONTROL
    // ==========================================
    
    // Pass a ConnectionInfo struct
    UFUNCTION(BlueprintCallable, Category = "Zenoh|Connection")
    bool Connect(const FZenohConnectionInfo& ConnectionInfo);

    UFUNCTION(BlueprintCallable, Category = "Zenoh|Connection")
    void Disconnect(FName ConnectionName);

    UFUNCTION(BlueprintCallable, Category = "Zenoh|Connection")
    void DisconnectAll();
    
    UFUNCTION(BlueprintPure, Category = "Zenoh|Connection")
    bool IsConnected(FName ConnectionName) const;

    // ==========================================
    // MESSAGING
    // ==========================================
    
    UFUNCTION(BlueprintCallable, Category = "Zenoh|Messaging")
    UZenohTopicListener* SubscribeToTopic(FName ConnectionName, FString Topic);

    UFUNCTION(BlueprintCallable, Category = "Zenoh|Messaging")
    bool Publish(FName ConnectionName, FString Topic, FString Message);

    UPROPERTY(BlueprintAssignable, Category = "Zenoh|Messaging")
    FOnZenohGlobalMessage OnGlobalMessageReceived;

private:
    void HandleZenohMessage(const FName& ConnectionName, const FString& Topic, const FString& Payload);
    
    // Multi-connection dictionary
    // Maps a name (ex. "Docker") to its specific C++ backend instance.
    TMap<FName, FZenohBackend*> ActiveConnections;

    // A Mutex Lock to prevent Background Threads and GameThreads from colliding
    mutable FCriticalSection ConnectionMapLock;

    // Dictionary of Delegates for specific topics. This allows Blueprints to bind custom events to specific topics, instead of using the global OnMessageReceived delegate and filtering by topic manually.
    UPROPERTY()
    TMap<FString, UZenohTopicListener*> TopicListeners;
    
};