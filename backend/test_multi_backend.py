import zenoh
import time
import threading

# Configuration
UNREAL_IP = "host.docker.internal"
PORT_A = 7447
PORT_B = 7448  # Use different ports to test strict network isolation

def run_guest(name, port):
    print(f"[{name}] Starting session on {UNREAL_IP}:{port}...")
    conf = zenoh.Config()
    conf.insert_json5("mode", "'client'")
    conf.insert_json5("connect/endpoints", f"['tcp/{UNREAL_IP}:{port}']")
    
    try:
        session = zenoh.open(conf)
    except Exception as e:
        print(f"[{name}] FAILED to connect: {e}")
        return
    
    # Listen for ACKs specifically for this guest
    ack_topic = f"sim/latency/ack/{name.lower()}"
    received_acks = []

    def ack_handler(sample):
        msg_id = sample.payload.to_string()
        print(f"[{name}] RECEIVED ACK: {msg_id}")
        received_acks.append(msg_id)

    sub = session.declare_subscriber(ack_topic, ack_handler)
    pub = session.declare_publisher(f"sim/test/{name.lower()}")

    print(f"[{name}] Connected. Sending test messages...")
    
    try:
        for i in range(5):
            # Format: [AckTopic]:[MsgId]|[Payload]
            # This is the envelope the ZenohWorkerThread parses
            payload = f"Hello from {name} - Packet {i}"
            envelope = f"{ack_topic}:{i}|{payload}".encode()
            
            pub.put(envelope)
            print(f"[{name}] SENT message {i} with ACK request to {ack_topic}")
            time.sleep(2)
    finally:
        sub.undeclare()
        session.close()
        print(f"[{name}] Closed.")

if __name__ == "__main__":
    # Run two guests in parallel threads to simulate two different machines/processes
    thread_a = threading.Thread(target=run_guest, args=("Guest_A", PORT_A))
    thread_b = threading.Thread(target=run_guest, args=("Guest_B", PORT_B))

    thread_a.start()
    thread_b.start()

    thread_a.join()
    thread_b.join()
