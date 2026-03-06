import zenoh
import json
import time
import mujoco
import numpy as np
import os
from world_builder import WorldBuilder

# ==========================================
# CONFIGURATION
# ==========================================
ZENOH_ENDPOINT = "tcp/host.docker.internal:7447"
INIT_TOPIC = "sim/world/init"
STATE_TOPIC = "sim/world/state" 
DEBUG_XML_PATH = "/app/debug_world.xml" 

POS_TOLERANCE = 0.001 
QUAT_TOLERANCE = 0.01

class StatePublisher:
    def __init__(self, zenoh_session):
        self.session = zenoh_session
        self.world_pub = self.session.declare_publisher(STATE_TOPIC)
        self.agent_pubs = {}
        self.last_state = {}

    def extract_and_publish(self, model, data, dynamic_props):
        world_payload = {"objects": []}
        agent_payloads = {}
        has_world_updates = False
        
        # 1. Update Floating Props
        for name in dynamic_props:
            try:
                mj_pos = data.body(name).xpos
                mj_quat = data.body(name).xquat 
                ue_pos = [mj_pos[0] * 100.0, mj_pos[1] * -100.0, mj_pos[2] * 100.0]
                ue_quat = [-mj_quat[1], mj_quat[2], -mj_quat[3], mj_quat[0]]
                
                if self._check_movement(name, ue_pos, ue_quat):
                    world_payload["objects"].append({"name": name, "pos": ue_pos, "quat": ue_quat})
                    self.last_state[name] = {'pos': ue_pos, 'quat': ue_quat}
                    has_world_updates = True
            except KeyError: pass
            
        # 2. Update Robot Joints
        for j in range(model.njnt):
            jnt_type = model.jnt_type[j]
            if jnt_type in [mujoco.mjtJoint.mjJNT_HINGE, mujoco.mjtJoint.mjJNT_SLIDE]:
                jnt_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_JOINT, j)
                if not jnt_name: continue
                    
                val = float(data.qpos[model.jnt_qposadr[j]])
                state_key = f"jnt_{jnt_name}"
                
                if state_key not in self.last_state or abs(val - self.last_state[state_key]) >= 0.001:
                    
                    # Identify which Agent owns this joint via the master wrapper suffix
                    body_id = model.jnt_bodyid[j]
                    agent_name = "unknown_agent"
                    while body_id != 0:
                        b_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_BODY, body_id)
                        if b_name and b_name.endswith("__root"):
                            agent_name = b_name.replace("__root", "")
                            break
                        body_id = model.body_parentid[body_id]

                    if agent_name not in agent_payloads:
                        agent_payloads[agent_name] = {"joints": {"hinge": {}, "slide": {}}}
                        
                    if jnt_type == mujoco.mjtJoint.mjJNT_HINGE:
                        agent_payloads[agent_name]["joints"]["hinge"][jnt_name] = -val # Left-Handed Flip
                    else:
                        agent_payloads[agent_name]["joints"]["slide"][jnt_name] = val
                        
                    self.last_state[state_key] = val
                    
        # Publish
        if has_world_updates:
            self.world_pub.put(json.dumps(world_payload))
            
        for agent_name, payload in agent_payloads.items():
            if agent_name not in self.agent_pubs:
                topic = f"sim/agent/{agent_name}/state"
                self.agent_pubs[agent_name] = self.session.declare_publisher(topic)
                print(f"[RoboSim] Opened dedicated channel: {topic}")
            self.agent_pubs[agent_name].put(json.dumps(payload))

    def _check_movement(self, name, new_pos, new_quat):
        if name not in self.last_state: return True
        old_pos = self.last_state[name]['pos']
        old_quat = self.last_state[name]['quat']
        if np.linalg.norm(np.array(new_pos) - np.array(old_pos)) > POS_TOLERANCE: return True
        if abs(np.dot(old_quat, new_quat)) < (1.0 - QUAT_TOLERANCE): return True
        return False

# ==========================================
# THE MAIN APPLICATION
# ==========================================
def main():
    print("[RoboSim] Starting Main Physics Node...")
    conf = zenoh.Config()
    conf.insert_json5("mode", "'client'")
    conf.insert_json5("connect/endpoints", f"['{ZENOH_ENDPOINT}']")
    session = zenoh.open(conf)
    
    state_pub = StatePublisher(session)
    physics_model = None
    physics_data = None
    dynamic_props = []
    is_ready = False

    def on_init(sample):
        nonlocal physics_model, physics_data, dynamic_props, is_ready
        print("\n[RoboSim] Received UE5 World Data! Building simulation...")
        
        json_payload = json.loads(sample.payload.to_string())
        builder = WorldBuilder(json_payload)
        xml_str, dynamic_props = builder.build()
        
        try:
            physics_model = mujoco.MjModel.from_xml_string(xml_str)
            physics_data = mujoco.MjData(physics_model)
            
            windows_debug_xml = xml_str.replace('/app/backend/assets/', 'assets/').replace('/app/assets/', 'assets/')
            with open(DEBUG_XML_PATH, "w") as f:
                f.write(windows_debug_xml)
                
            print("[RoboSim] SUCCESS! Physics Engine Online.")
            is_ready = True
        except Exception as e:
            print(f"[RoboSim] FATAL ERROR Loading MuJoCo: {e}")

    sub = session.declare_subscriber(INIT_TOPIC, on_init)
    print(f"[RoboSim] Waiting for Unreal Engine on '{INIT_TOPIC}'...")

    while not is_ready:
        time.sleep(0.1)
        
    sub.undeclare() 
    print("[RoboSim] Entering Main Physics Loop (60 FPS)...")
    
    frame_time = 1.0 / 60.0 
    try:
        while True:
            step_start = time.perf_counter()
            
            for _ in range(8):
                mujoco.mj_step(physics_model, physics_data)
                
            state_pub.extract_and_publish(physics_model, physics_data, dynamic_props)
            
            sleep_time = frame_time - (time.perf_counter() - step_start)
            if sleep_time > 0: 
                time.sleep(sleep_time)
                
    except KeyboardInterrupt:
        print("\n[RoboSim] Simulation Stopped safely.")

if __name__ == "__main__":
    main()