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
void zenoh_pimpl_callback(z_loaned_sample_t* sample, void* context)
{
    if (!sample || !context) return;

    // 1. Convert payload to a Zenoh string object
    z_owned_string_t payload_string;
    z_bytes_to_string(z_sample_payload(sample), &payload_string);

    // 2. Get the exact data pointer and length safely
    const char* data = z_string_data(z_loan(payload_string));
    size_t len = z_string_len(z_loan(payload_string));

    // 3. Convert to Unreal String safely using exact length (Ignores garbage memory)
    FUTF8ToTCHAR StringConverter(data, (int32)len);
    FString Msg(StringConverter.Length(), StringConverter.Get());

    // CRITICAL: Clean up memory before proceeding
    z_drop(z_move(payload_string));

    // Cast the context back to our C++ callback
    auto* CallbackPtr = static_cast<FZenohBackend::FOnMessageCallback*>(context);
    
    if (CallbackPtr)
    {
        // FIX: The Race Condition.
        // Copy the TFunction by value right now, while we are absolutely sure 
        // the pointer is still alive on this Zenoh background thread.
        FZenohBackend::FOnMessageCallback SafeCallback = *CallbackPtr;

        // Jump to the GameThread passing the COPY, not the pointer!
        AsyncTask(ENamedThreads::GameThread, [SafeCallback, Msg]()
        {
            SafeCallback(Msg);
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

    // 1. Initialize configuration (Pass by reference for Zenoh 1.7.2)
    struct z_owned_config_t config;
    z_config_default(&config);

    // 2. Set Mode (client or peer) using the generic z_loan_mut macro
    zc_config_insert_json5(z_loan_mut(config), "mode", StdMode.c_str());

    // 3. Set Endpoint dynamically!
    FString ConfigKey = (Mode == TEXT("client")) ? TEXT("connect/endpoints") : TEXT("listen/endpoints");
    std::string StdConfigKey = std::string(TCHAR_TO_UTF8(*ConfigKey));

    // Format the JSON5 array string for the endpoint: "['tcp/127.0.0.1:7447']"
    std::string JsonEndpoint = "['" + StdEndpoint + "']";

    zc_config_insert_json5(z_loan_mut(config), StdConfigKey.c_str(), JsonEndpoint.c_str());

    // 4. Open the session
    if (z_open(&State->Session, z_move(config), nullptr) < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh] Failed to open Zenoh Session."));
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

    // Safely convert Unreal FString to UTF-8
    FTCHARToUTF8 StdTopic(*Topic);
    FTCHARToUTF8 StdMsg(*Message);

    // 1. Create a "View" of the Key Expression (Read-only, no allocation/drop needed!)
    z_view_keyexpr_t key_expr;
    if (z_view_keyexpr_from_str(&key_expr, StdTopic.Get()) < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh ERROR] Invalid Topic Name (No spaces allowed): '%s'"), *Topic);
        return false;
    }

    // 2. Create an "Owned" Payload (This strictly fixes the Access Violation Crash!)
    z_owned_bytes_t payload;
    z_bytes_copy_from_str(&payload, StdMsg.Get());

    // 3. Put data to the network (Passing NULL for options uses the defaults)
    if (z_put(z_loan(State->Session), z_loan(key_expr), z_move(payload), NULL) < 0)
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

    FTCHARToUTF8 TopicUTF(*Topic);
    z_view_keyexpr_t key_expr;
    if (z_view_keyexpr_from_str(&key_expr, TopicUTF.Get()) < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh ERROR] Invalid Subscribe Topic: '%s'"), *Topic);
        delete HeapCallback; // Clean up on failure
        return;
    }

    // Use the simplified generic z_closure macro exactly as shown in the docs
    z_owned_closure_sample_t callback;
    z_closure(&callback, zenoh_pimpl_callback, zenoh_pimpl_drop, HeapCallback);

    // Declare subscriber using the generic z_loan macro and NULL for default options
    if (z_declare_subscriber(z_loan(State->Session), &State->Subscriber, z_loan(key_expr), z_move(callback), NULL) < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh ERROR] Failed to subscribe to network."));
        
        // If declaration fails, Zenoh doesn't take ownership of the closure.
        // Calling z_drop automatically triggers zenoh_pimpl_drop, safely deleting our HeapCallback!
        z_drop(z_move(callback)); 
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[Zenoh] Successfully subscribed to: %s"), *Topic);
    }
}