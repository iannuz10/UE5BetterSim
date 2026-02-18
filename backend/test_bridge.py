import zenoh
import json
import time
import math

def main():
    conf = zenoh.Config()
    print("[Brain] Connecting to Windows Unreal Engine...")
    conf.insert_json5("connect/endpoints", "['tcp/host.docker.internal:7447']")
    print("[Brain] Opening Zenoh session...")
    session = zenoh.open(conf)
    
    key = "sim/command"
    pub = session.declare_publisher(key)
    
    t = 0.0
    print(f"[Brain] Sending sine wave position to {key}...")
    
    while True:
        t += 0.05
        
        # Move in a circle radius 200cm
        x = math.cos(t) * 20.0
        y = math.sin(t) * 20.0
        z = (math.sin(t * 2.0) * 5.0) # Bob up and down
        
        # Strict JSON format matching C++ expectations
        message = {
            "location": {
                "x": x,
                "y": y,
                "z": z
            }
        }
        
        json_str = json.dumps(message)
        pub.put(json_str)
        
        # 60 Hz update rate
        time.sleep(0.016)

if __name__ == "__main__":
    main()