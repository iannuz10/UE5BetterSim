import zenoh
import time
import json

def listener(sample):
    payload_bytes = bytes(sample.payload)
    data = json.loads(payload_bytes.decode('utf-8'))
    print(f"[Windows] Received: x={data['x']}, msg={data['message']}")

def main():
    print("[Windows] Opening Zenoh session...")
    conf = zenoh.Config()

    conf.insert_json5("connect/endpoints", "['tcp/127.0.0.1:7447']")

    session = zenoh.open(conf)

    key = "sim/command"
    print(f"[Windows] Subscribing to {key}...")

    sub = session.declare_subscriber(key, listener)

    while True:
        time.sleep(1)

if __name__ == "__main__":
    main()