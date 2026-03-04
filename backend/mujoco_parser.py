import zenoh
import json
import time
import mujoco
import numpy as np
import world_builder 

ZENOH_ENDPOINT = "tcp/host.docker.internal:7447"
INIT_TOPIC = "sim/world/init"
STATE_TOPIC = "sim/world/state" 
DEBUG_XML_PATH = "/app/debug_world.xml" 

class StatePublisher:
    def __init__(self, session):
        self.session = session
        self.world_pub = session.declare_publisher(STATE_TOPIC)
        self.agent_pubs = {}
        self.last_state = {}

    def extract_and_publish(self, model, data, dynamic_props):
        world_payload = {"objects": []}
        agent_payloads = {}
        has_world_updates = False
        
        # Props
        for name in dynamic_props:
            try:
                mj_pos, mj_quat = data.body(name).xpos, data.body(name).xquat 
                ue_pos = [mj_pos[0] * 100.0, mj_pos[1] * -100.0, mj_pos[2] * 100.0]
                ue_quat = [-mj_quat[1], mj_quat[2], -mj_quat[3], mj_quat[0]]
                
                if self._check_movement(name, ue_pos, ue_quat):
                    world_payload["objects"].append({"name": name, "pos": ue_pos, "quat": ue_quat})
                    self.last_state[name] = {'pos': ue_pos, 'quat': ue_quat}
                    has_world_updates = True
            except KeyError: pass
            
        # Agent Joints
        for j in range(model.njnt):
            if model.jnt_type[j] in [mujoco.mjtJoint.mjJNT_HINGE, mujoco.mjtJoint.mjJNT_SLIDE]:
                jnt_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_JOINT, j)
                if not jnt_name: continue
                    
                val = float(data.qpos[model.jnt_qposadr[j]])
                
                if f"jnt_{jnt_name}" not in self.last_state or abs(val - self.last_state[f"jnt_{jnt_name}"]) >= 0.001:
                    body_id = model.jnt_bodyid[j]
                    agent_name = "unknown_agent"
                    
                    while body_id != 0:
                        b_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_BODY, body_id)
                        if b_name and b_name.endswith("__root"):
                            agent_name = b_name.split("__root")[0]
                            break
                        body_id = model.body_parentid[body_id]

                    if agent_name not in agent_payloads:
                        agent_payloads[agent_name] = {"joints": {"hinge": {}, "slide": {}}}
                        
                    if model.jnt_type[j] == mujoco.mjtJoint.mjJNT_HINGE:
                        agent_payloads[agent_name]["joints"]["hinge"][jnt_name] = -val
                    else:
                        agent_payloads[agent_name]["joints"]["slide"][jnt_name] = val
                        
                    self.last_state[f"jnt_{jnt_name}"] = val
                    
        # Publish
        if has_world_updates:
            self.world_pub.put(json.dumps(world_payload))
            
        for agent_name, payload in agent_payloads.items():
            if agent_name not in self.agent_pubs:
                self.agent_pubs[agent_name] = self.session.declare_publisher(f"sim/agent/{agent_name}/state")
            self.agent_pubs[agent_name].put(json.dumps(payload))

    def _check_movement(self, name, new_pos, new_quat):
        if name not in self.last_state: return True
        if np.linalg.norm(np.array(new_pos) - np.array(self.last_state[name]['pos'])) > 0.001: return True
        if abs(np.dot(new_quat, self.last_state[name]['quat'])) < 0.99: return True
        return False

def main():
    print("[RoboSim] Starting MuJoCo Node...")
    session = zenoh.open(zenoh.Config.from_json5(f'{{"mode": "client", "connect": {{"endpoints": ["{ZENOH_ENDPOINT}"]}}}}'))
    state_pub = StatePublisher(session)
    
    physics_model, physics_data, dynamic_props, is_ready = None, None, [], False

    def on_init(sample):
        nonlocal physics_model, physics_data, dynamic_props, is_ready
        xml_str, dynamic_props = world_builder.build_world(json.loads(sample.payload.to_string()))
        
        # 1. Boot Docker Simulation directly from memory strings!
        try:
            physics_model = mujoco.MjModel.from_xml_string(xml_str)
            physics_data = mujoco.MjData(physics_model)
            print("[RoboSim] SUCCESS! Physics Engine Online.")
            is_ready = True
        except Exception as e:
            print(f"[RoboSim] FATAL ERROR Loading MuJoCo: {e}")
            
        # 2. Write a clean, relative-path XML purely for Windows debugging
        windows_xml = xml_str.replace('/app/assets/', 'assets/').replace('/app/backend/assets/', 'assets/')
        with open(DEBUG_XML_PATH, "w") as f:
            f.write(windows_xml)

    sub = session.declare_subscriber(INIT_TOPIC, on_init)
    
    while not is_ready: time.sleep(0.1)
    sub.undeclare() 
    
    print("[RoboSim] Running Physics Loop (60 FPS)...")
    frame_time = 1.0 / 60.0 
    try:
        while True:
            step_start = time.perf_counter()
            for _ in range(8): mujoco.mj_step(physics_model, physics_data)
            state_pub.extract_and_publish(physics_model, physics_data, dynamic_props)
            
            sleep_time = frame_time - (time.perf_counter() - step_start)
            if sleep_time > 0: time.sleep(sleep_time)
    except KeyboardInterrupt:
        print("\n[RoboSim] Stopped safely.")

if __name__ == "__main__":
    main()