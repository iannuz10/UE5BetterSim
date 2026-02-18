import zenoh
import json
import time

def listener(sample):
    # 1. Convert Payload to basic String
    payload_bytes = bytes(sample.payload)
    raw_str = payload_bytes.decode('utf-8')
    
    # 2. Try parsing as JSON, otherwise print as text
    try:
        data = json.loads(raw_str)
        print(f"[Windows] JSON Received: {data}")
    except json.JSONDecodeError:
        print(f"[Windows] Text Received: {raw_str}")

def main():
    conf = zenoh.Config()
    # Force Windows to CONNECT to Docker on localhost:7447
    conf.insert_json5("connect/endpoints", "['tcp/127.0.0.1:7447']")
    
    print("[Windows] Opening Zenoh session (Mode: Client)...")
    session = zenoh.open(conf)
    
    key = "sim/command"
    print(f"[Windows] Subscribing to {key}...")
    sub = session.declare_subscriber(key, listener)
    
    while True:
        time.sleep(1)

if __name__ == "__main__":
    main()