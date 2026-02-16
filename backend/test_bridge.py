import zenoh
import time
import json

def test_bridge():
    conf = zenoh.Config()
    print("[Docker] Opeing Zenoh session...")
    session = zenoh.open(conf)

    key = "sim/command"

    print(f"[Docker] Publishing to {key}")

    count = 0

    while True:
        payload = {
            "x" : count % 10,
            "y" : 0,
            "z" : 0,
            "message" : "Hello from sim_brain container!"
        }

        session.put(key, json.dumps(payload))

        print(f"[Docker] Sent: {payload}")
        count += 1
        time.sleep(1)

if __name__ == "__main__":
    test_bridge()