import zenoh
import time
import json
import statistics 

# CONFIGURATION
PING_TOPIC = "sim/ping"
PONG_TOPIC = "sim/pong"
NUM_SAMPLES = 1000 

def main():
    # 1. SETUP
    conf = zenoh.Config()
    conf.insert_json5("connect/endpoints", "['tcp/host.docker.internal:7447']")
    print(f"[Benchmark] Connecting to Unreal...")
    session = zenoh.open(conf)

    # 2. PREPARE PUB/SUB
    pub = session.declare_publisher(PING_TOPIC)
    
    # Use a "Promise" pattern here. Wait for a reply.
    # A simple list to store the RTTs
    latencies_ms = []

    print(f"[Benchmark] Starting RTT test ({NUM_SAMPLES} samples)...")
    print("------------------------------------------------")

    # 3. THE LOOP
    for i in range(NUM_SAMPLES):
        
        # Setup the listener for THIS specific round
        # Create a temporary subscriber that waits for ONE message then dies.
        # This prevents old messages from messing up new measurements.
        reply_received = False
        
        def on_pong(sample):
            nonlocal reply_received
            reply_received = True

        sub = session.declare_subscriber(PONG_TOPIC, on_pong)

        # Mark Start Time
        t0 = time.perf_counter()

        # Send Ping
        pub.put("PING")

        # Wait for Pong (Busy wait for max precision in Python)
        # Note: In production C++, we'd use Condition Variables, but for a script, this is fine.
        while not reply_received:
            time.sleep(0.00001) # Check every 10 microseconds
            # Safety timeout (1 second)
            if time.perf_counter() - t0 > 1.0:
                print(f"Sample {i}: TIMEOUT")
                break
        
        # Mark End Time
        t1 = time.perf_counter()
        
        # Calculate RTT (Seconds -> Milliseconds)
        rtt_ms = (t1 - t0) * 1000.0
        latencies_ms.append(rtt_ms)
        
        # Cleanup subscriber
        sub.undeclare()

        # Optional: Print every 100th sample to show life
        if i % 100 == 0:
            print(f"Sample {i}: {rtt_ms:.3f} ms")

    # REPORT CARD
    print("------------------------------------------------")
    print(f"RESULTS ({NUM_SAMPLES} samples)")
    print(f"MIN: {min(latencies_ms):.3f} ms")
    print(f"MAX: {max(latencies_ms):.3f} ms")
    print(f"AVG: {statistics.mean(latencies_ms):.3f} ms")
    print(f"P99: {sorted(latencies_ms)[int(len(latencies_ms)*0.99)]:.3f} ms")
    print("------------------------------------------------")
    print(f"One-way Latency (Approx): {statistics.mean(latencies_ms)/2:.3f} ms")

if __name__ == "__main__":
    main()