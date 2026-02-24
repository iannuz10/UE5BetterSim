#include "ZenohBackend.h"
#include "Containers/StringConv.h"
#include "Async/Async.h"
#include "String"

// --- SAFE MACRO MANAGEMENT ---

// 1. Platform-Specific Headers
#if PLATFORM_WINDOWS
    #include "Windows/AllowWindowsPlatformTypes.h"
#endif

// 2. Disable Warnings (Common for both)
#pragma warning(push)
#pragma warning(disable: 4191) 
#pragma warning(disable: 4005)

// 3. Handle ALIGN conflict
// Unreal defines ALIGN. Zenoh defines ALIGN.
#ifdef ALIGN
    #define UNREAL_ALIGN ALIGN
    #undef ALIGN
#endif

// 4. Include Zenoh
#include "zenoh.h" 

// 5. Restore ALIGN
#ifdef ALIGN
    #undef ALIGN // remove Zenoh's
#endif

#ifdef UNREAL_ALIGN
    #define ALIGN UNREAL_ALIGN
    #undef UNREAL_ALIGN
#endif

#undef ZENOHC_API

// 6. Restore Platform Headers
#if PLATFORM_WINDOWS
    #pragma warning(pop)
    #include "Windows/HideWindowsPlatformTypes.h"
#else
    #pragma warning(pop)
#endif
// ------------------------------

// This struct lives ONLY inside this .cpp file
struct FZenohState
{
    struct z_owned_session_t Session;
    struct z_owned_subscriber_t Subscriber;
    bool bInitialized = false;
};

// --- CALLBACK WRAPPER ---
void zenoh_pimpl_callback(struct z_loaned_sample_t* sample, void* context)
{
    // 1. Get Payload
    const struct z_loaned_bytes_t* payload = z_sample_payload(sample);
    size_t len = z_bytes_len(payload);
    
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(len + 1);
    
    struct z_bytes_reader_t reader = z_bytes_get_reader(payload);
    z_bytes_reader_read(&reader, Buffer.GetData(), len);
    Buffer[len] = 0;

    FString Msg = UTF8_TO_TCHAR((const char*)Buffer.GetData());

    // 2. Execute the user's callback
    auto* CallbackPtr = static_cast<FZenohBackend::FOnMessageCallback*>(context);
    if (CallbackPtr)
    {
        // Execute on Game Thread
        AsyncTask(ENamedThreads::GameThread, [CallbackPtr, Msg]()
        {
            (*CallbackPtr)(Msg);
        });
    }
}

FZenohBackend::FZenohBackend()
{
    State = new FZenohState();
}

FZenohBackend::~FZenohBackend()
{
    Shutdown();
    delete State;
}

bool FZenohBackend::Initialize()
{
    // 1. Create Config
    struct z_owned_config_t config;
    z_config_default(&config);

    // --- FIX: FORCE CONNECTION TO DOCKER ---
    // Tell Zenoh to connect to localhost:7447 (where Docker is listening) (TESTING ONLY)
    // Borrow the config mutably to modify it
    if (zc_config_insert_json5(z_config_loan_mut(&config), "listen/endpoints", "['tcp/0.0.0.0:7447']") < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Zenoh] Failed to set listen endpoint config!"));
    }
    // ---------------------------------------

    struct z_open_options_t options;
    z_open_options_default(&options);

    // 2. Open Session
    if (z_open(&State->Session, (struct z_moved_config_t*)&config, &options) < 0)
    {
        return false;
    }
    State->bInitialized = true;
    return true;
}

bool FZenohBackend::Initialize(const FString& Mode, const FString& Endpoint)
{
    State = new FZenohState();
    
    // Convert Unreal Strings to standard C strings
    std::string StdMode = std::string(TCHAR_TO_UTF8(*Mode));
    std::string StdEndpoint = std::string(TCHAR_TO_UTF8(*Endpoint));

    // 1. Initialize configuration
    struct z_owned_config_t config;
    z_config_default(&config);

    // 2. Set Mode (client or peer)
    zc_config_insert_json5(z_config_loan_mut(&config), "mode", StdMode.c_str());

    // 3. Set Endpoint dynamically!
    // Note: If we are a Peer, we 'listen'. If we are a Client, we 'connect'.
    // Since UE5 is usually the server/host, we will use listen/endpoints for now, 
    // but you can expand this logic later.
    FString ConfigKey = (Mode == TEXT("client")) ? TEXT("connect/endpoints") : TEXT("listen/endpoints");
    std::string StdConfigKey = std::string(TCHAR_TO_UTF8(*ConfigKey));

    // Format the JSON5 array string for the endpoint: "['tcp/127.0.0.1:7447']"
    std::string JsonEndpoint = "['" + StdEndpoint + "']";

    zc_config_insert_json5(z_config_loan_mut(&config), StdConfigKey.c_str(), JsonEndpoint.c_str());

    // 4. Open the session
    if (z_open(&State->Session, z_move(config), nullptr) < 0)
    {
        return false;
    }

    return true;
}

void FZenohBackend::Shutdown()
{
    if (State && State->bInitialized)
    {
        // Close Session
        z_close(z_session_loan_mut(&State->Session), NULL);
        z_session_drop((struct z_moved_session_t*)&State->Session);
        State->bInitialized = false;
    }
}
/*
bool FZenohBackend::Publish(const FString& Topic, const FString& Message)
{
    if (!State || !State->bInitialized) return false;

    struct z_put_options_t options;
    z_put_options_default(&options);
    
    FTCHARToUTF8 TopicUTF(*Topic);
    struct z_view_keyexpr_t key;
    z_view_keyexpr_from_str(&key, TopicUTF.Get());

    FTCHARToUTF8 MsgUTF(*Message);
    struct z_owned_bytes_t payload;
    z_bytes_copy_from_str(&payload, MsgUTF.Get());

    if (z_put(z_session_loan(&State->Session), z_view_keyexpr_loan(&key), (struct z_moved_bytes_t*)&payload, &options) < 0)
    {
        return false;
    }
    return true;
} */

bool FZenohBackend::Publish(const FString& Topic, const FString& Message)
{
    if (!State) return false;

    std::string StdTopic(TCHAR_TO_UTF8(*Topic));
    std::string StdMsg(TCHAR_TO_UTF8(*Message));

    // FIX 1: Bypass the MSVC macro bug by formally declaring an owned key expression
    struct z_view_keyexpr_t key;
    if (z_view_keyexpr_from_str(&key, StdTopic.c_str()) < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh ERROR] Invalid Topic Name (No spaces allowed): '%s'"), *Topic);
        return false;
    }

    // Initialize options by passing the pointer 
    z_put_options_t options;
    z_put_options_default(&options);

    FTCHARToUTF8 MsgUTF(*Message);
    struct z_owned_bytes_t payload;
    z_bytes_copy_from_str(&payload, MsgUTF.Get());
    
    if (z_put(z_session_loan(&State->Session), z_view_keyexpr_loan(&key), z_move(payload), &options) <0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh ERROR] Failed to put data on network."));
        return false;
    }
    return true;
}

/*
void FZenohBackend::Subscribe(const FString& Topic, FOnMessageCallback Callback)
{
    if (!State || !State->bInitialized) return;

    // Store callback on heap so Zenoh can hold onto it
    auto* HeapCallback = new FOnMessageCallback(Callback);

    struct z_subscriber_options_t sub_opts;
    z_subscriber_options_default(&sub_opts);

    struct z_view_keyexpr_t key;
    FTCHARToUTF8 TopicUTF(*Topic);
    z_view_keyexpr_from_str(&key, TopicUTF.Get());

    struct z_owned_closure_sample_t closure;

    z_closure_sample(&closure, zenoh_pimpl_callback, NULL, HeapCallback);

    z_declare_subscriber(z_session_loan(&State->Session), &State->Subscriber, z_view_keyexpr_loan(&key), (struct z_moved_closure_sample_t*)&closure, &sub_opts);
}
*/

void zenoh_pimpl_drop(void* context)
{
    auto* CallbackPtr = static_cast<FZenohBackend::FOnMessageCallback*>(context);
    if (CallbackPtr)
    {
        delete CallbackPtr;
    }
}

void FZenohBackend::Subscribe(const FString& Topic, FOnMessageCallback Callback)
{
    if (!State) return;

    // Save the callback in the backend instance so it stays alive in memory
    auto* HeapCallback = new FOnMessageCallback(Callback);

    z_subscriber_options_t options;
    z_subscriber_options_default(&options);

    z_view_keyexpr_t key;
    FTCHARToUTF8 TopicUTF(*Topic);
    if (z_view_keyexpr_from_str(&key, TopicUTF.Get()) < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh ERROR] Invalid Subscribe Topic: '%s'"), *Topic);
        delete HeapCallback; // Clean up on failure
        return;
    }

    // FIX 1: The closure initialization was accidentally commented out! Moved it to a new line.
    // We also pass zenoh_pimpl_drop so Zenoh knows how to clean up the HeapCallback.
    z_owned_closure_sample_t closure; 
    z_closure_sample(&closure, zenoh_pimpl_callback, zenoh_pimpl_drop, HeapCallback);

    // FIX 2: Use the typed loan functions you correctly found for MSVC
    if (z_declare_subscriber(z_session_loan(&State->Session), &State->Subscriber, z_view_keyexpr_loan(&key), z_move(closure), &options) < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh ERROR] Failed to subscribe to network."));
        // Note: If declaration fails, Zenoh doesn't take ownership of the closure, so we must clean up the heap allocation manually
        delete HeapCallback;
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[Zenoh] Successfully subscribed to: %s"), *Topic);
    }
}