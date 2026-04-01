import zenoh
import json
import time
import mujoco
import numpy as np
import os
from world_builder import WorldBuilder
import logging

# ==========================================
# TELEMETRY SETUP
# ==========================================
# Wipes the log on every boot so we don't end up with a 50GB text file.
logging.basicConfig(
    filename='mujoco_run.log', 
    filemode='w', 
    level=logging.DEBUG, # Captures EVERYTHING (debug, info, warning, error)
    format='[%(asctime)s] [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)

logger = logging.getLogger("MuJoCo_Core")
logger.info("Starting MuJoCo Physics Backend...")

# ==========================================
# CONFIGURATION
# ==========================================
# TODO: Move these to a .env file or pass via command line args. Hardcoding Docker host IPs doesn't scale.
ZENOH_ENDPOINT = "tcp/host.docker.internal:7447"
INIT_TOPIC = "sim/world/init"
STATE_TOPIC = "sim/world/state" 
DEBUG_XML_PATH = "/app/debug_world.xml" 

# TODO: Add a topic to receive the end state from Unreal Engine so we can stop the simulation.

# Delta thresholds to prevent network spam if the robot is just vibrating slightly.
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
        
        # --- 1. PROPS ---
        for name in dynamic_props:
            try:
                mj_pos = data.body(name).xpos
                mj_quat = data.body(name).xquat 

                # Math Transform: Right-Handed (MuJoCo) -> Left-Handed (UE5) + Meters to cm.
                ue_pos = [mj_pos[0] * 100.0, mj_pos[1] * -100.0, mj_pos[2] * 100.0]
                ue_quat = [-mj_quat[1], mj_quat[2], -mj_quat[3], mj_quat[0]]
                
                if self._check_movement(name, ue_pos, ue_quat):
                    world_payload["objects"].append({"name": name, "pos": ue_pos, "quat": ue_quat})
                    self.last_state[name] = {'pos': ue_pos, 'quat': ue_quat}
                    has_world_updates = True
            except KeyError: pass
            
        # --- 2. ROBOT JOINTS ---
        for j in range(model.njnt):
            jnt_type = model.jnt_type[j]
            if jnt_type in [mujoco.mjtJoint.mjJNT_HINGE, mujoco.mjtJoint.mjJNT_SLIDE]:
                jnt_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_JOINT, j)
                if not jnt_name: continue
                    
                val = float(data.qpos[model.jnt_qposadr[j]])
                state_key = f"jnt_{jnt_name}"
                
                # Only publish if the joint actually moved
                if state_key not in self.last_state or abs(val - self.last_state[state_key]) >= 0.001:
                    
                    # Hacky tree traversal to find which robot owns this joint.
                    # TODO: Cache this mapping on boot instead of traversing the tree every frame!
                    body_id = model.jnt_bodyid[j]
                    agent_name = None
                    while body_id != 0:
                        b_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_BODY, body_id)
                        if b_name and b_name.endswith("__root"):
                            agent_name = b_name.replace("__root", "")
                            break
                        body_id = model.body_parentid[body_id]
                    
                    if agent_name is None:
                        logger.critical(f"FATAL: Joint '{jnt_name}' is orphaned! It does not belong to any valid Agent wrapper.")
                        raise RuntimeError(f"Orphaned joint detected: {jnt_name}")

                    if agent_name not in agent_payloads:
                        agent_payloads[agent_name] = {"joints": {"hinge": {}, "slide": {}}}
                    
                    # Flip hinge rotations for UE5's Left-Handed coordinate system. Slide joints are unaffected.
                    if jnt_type == mujoco.mjtJoint.mjJNT_HINGE:
                        agent_payloads[agent_name]["joints"]["hinge"][jnt_name] = -val # Left-Handed Flip
                    else:
                        agent_payloads[agent_name]["joints"]["slide"][jnt_name] = val
                        
                    self.last_state[state_key] = val
                    
        # --- 3. PUBLISH ---
        if has_world_updates:
            try:
                self.world_pub.put(json.dumps(world_payload))
            except Exception as e:
                logger.error(f"Failed to encode/publish world payload: {e}")
            
        for agent_name, payload in agent_payloads.items():
            if agent_name not in self.agent_pubs:
                # Dynamically create channels for new robots as they appear in the simulation. This allows for multiple agents without predefining them.
                topic = f"sim/agent/{agent_name}/state"
                self.agent_pubs[agent_name] = self.session.declare_publisher(topic)
                logger.info(f"Opened dedicated Zenoh channel: {topic}")
            try:
                self.agent_pubs[agent_name].put(json.dumps(payload))
            except Exception as e:
                logger.error(f"Failed to encode/publish agent payload for {agent_name}: {e}")

    def _check_movement(self, name, new_pos, new_quat):
        # Quick tolerance check to avoid publishing microslips
        if name not in self.last_state: return True
        old_pos = self.last_state[name]['pos']
        old_quat = self.last_state[name]['quat']
        if np.linalg.norm(np.array(new_pos) - np.array(old_pos)) > POS_TOLERANCE: return True
        if abs(np.dot(old_quat, new_quat)) < (1.0 - QUAT_TOLERANCE): return True
        return False

# ==========================================
# MAIN APP
# ==========================================
def main():
    logger.info("Initializing Zenoh Router Connection...")
    try:
        conf = zenoh.Config()
        conf.insert_json5("mode", "'client'")
        conf.insert_json5("connect/endpoints", f"['{ZENOH_ENDPOINT}']")
        session = zenoh.open(conf)
        logger.info(f"Successfully connected to Zenoh at {ZENOH_ENDPOINT}")
    except Exception as e:
        logger.critical(f"FATAL: Could not connect to Zenoh router. Error: {e}")
        return
    
    state_pub = StatePublisher(session)
    physics_model = None
    physics_data = None
    dynamic_props = []
    is_ready = False

    def on_init(sample):
        nonlocal physics_model, physics_data, dynamic_props, is_ready
        logger.info("Received INIT payload from Unreal Engine. Building simulation...")
        
        try:
            json_payload = json.loads(sample.payload.to_string())
        except json.JSONDecodeError as e:
            logger.critical(f"FATAL: Malformed JSON received on init topic. Error: {e}")
            return
        
        builder = WorldBuilder(json_payload)
        
        try:
            xml_str, dynamic_props = builder.build()
        except Exception as e:
            logger.critical(f"WorldBuilder crashed during compilation: {e}")
            return
        
        try:
            physics_model = mujoco.MjModel.from_xml_string(xml_str)
            physics_data = mujoco.MjData(physics_model)
            
            # Dump the stitched XML for debugging
            windows_debug_xml = xml_str.replace('/app/backend/assets/', 'assets/').replace('/app/assets/', 'assets/')
            with open(DEBUG_XML_PATH, "w") as f:
                f.write(windows_debug_xml)
                logger.info(f"Debug XML saved to {DEBUG_XML_PATH}")
            logger.info("SUCCESS! MuJoCo Physics Engine is Online and compiled.")
            is_ready = True
        except Exception as e:
            logger.critical(f"FATAL ERROR Loading MuJoCo Physics Model: {e}")

    sub = session.declare_subscriber(INIT_TOPIC, on_init)
    logger.info(f"Waiting for Unreal Engine on topic '{INIT_TOPIC}'...")

    while not is_ready:
        time.sleep(0.1)
        
    sub.undeclare() 
    logger.info("Entering Main Physics Loop (60 FPS)...")
    
    frame_time = 1.0 / 60.0 
    frame_counter = 0
    fallen_objects = set()

    try:
        # TODO [ARCHITECTURE]: This is a naive async spin-loop. 
        # For true digital twinning, we need a Lockstep/Event-driven architecture where 
        # Unreal Engine or the real hardware clock drives the tick, not `time.sleep()`.
        while True:
            step_start = time.perf_counter()
            
            # Step physics 8 times per render frame for stability, but this is a naive approach. A more robust solution would be to step based on actual elapsed time and handle variable frame rates.
            for _ in range(8):
                mujoco.mj_step(physics_model, physics_data)
                
            state_pub.extract_and_publish(physics_model, physics_data, dynamic_props)

            # --- TELEMETRY: ANOMALY DETECTOR ---
            frame_counter += 1
            if frame_counter % 60 == 0:
                # Catch physics explosions (usually bad collision meshes)
                if np.any(np.abs(physics_data.qvel) > 100.0):
                    logger.warning("PHYSICS ANOMALY: Extreme joint velocity detected. Possible collision explosion or NaN values.")
                
                # Catch objects that clipped through the floor
                for i in range(1, physics_model.nbody):
                    pos_z = physics_data.xpos[i][2]
                    if pos_z < -20.0:
                        b_name = mujoco.mj_id2name(physics_model, mujoco.mjtObj.mjOBJ_BODY, i)
                        if b_name and b_name not in fallen_objects:
                            logger.warning(f"PHYSICS ANOMALY: Body '{b_name}' fell out of bounds (Z = {pos_z:.2f}m).")
                            fallen_objects.add(b_name)
                            
                # Pipe MuJoCo's internal C-level warnings to our Python log
                for i in range(mujoco.mjtWarning.mjNWARNING):
                    if physics_data.warning[i].number > 0:
                        logger.error(f"MUJOCO NATIVE WARNING (Type {i}): Triggered {physics_data.warning[i].number} times.")
                        physics_data.warning[i].number = 0
            
            # Sleep to maintain ~60Hz (prone to OS scheduler drift)
            sleep_time = frame_time - (time.perf_counter() - step_start)
            if sleep_time > 0: 
                time.sleep(sleep_time)
                
    except KeyboardInterrupt:
        logger.info("Simulation Stopped safely via KeyboardInterrupt.")
    except Exception as e:
        logger.critical(f"FATAL RUNTIME ERROR in Physics Loop: {e}")

if __name__ == "__main__":
    main()