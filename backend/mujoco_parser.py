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

ACK_TOPIC_PREFIX = "sim/latency/ack"  # Full topic: sim/latency/ack/{agent_name}
RTT_LOG_WINDOW = 60  # Log RTT stats every N acknowledged messages (~1 second at 60Hz)

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
        self._msg_counter = 0
        self._pending_acks = {}  # {msg_id: send_time_ns}
        self._rtt_stats = {}     # {agent_name: {"samples": []}}
        self._ack_subs = {}      # {agent_name: zenoh subscriber}
        self._joint_agent_map = {}  # {joint_index: (agent_name, joint_name_str)}

    def build_joint_agent_map(self, model):
        """Build a one-time lookup dict: joint_index → (agent_name, joint_name).

        Replaces the per-frame kinematic tree traversal in extract_and_publish.
        Must be called once after WorldBuilder constructs the MuJoCo model,
        and again if on_init fires a second time (model rebuild).
        """
        self._joint_agent_map = {}
        agent_names_found = set()

        for j in range(model.njnt):
            jnt_type = model.jnt_type[j]
            if jnt_type not in [mujoco.mjtJoint.mjJNT_HINGE, mujoco.mjtJoint.mjJNT_SLIDE]:
                continue

            jnt_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_JOINT, j)
            if not jnt_name:
                continue

            # Walk the kinematic tree once to find the owning agent's __root body.
            body_id = model.jnt_bodyid[j]
            agent_name = None
            while body_id != 0:
                b_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_BODY, body_id)
                if b_name and b_name.endswith("__root"):
                    agent_name = b_name[:-len("__root")]
                    break
                body_id = model.body_parentid[body_id]

            if agent_name is None:
                logger.critical(
                    f"FATAL: Joint '{jnt_name}' is orphaned — no '__root' ancestor found. "
                    f"Check WorldBuilder wrapping logic."
                )
                raise RuntimeError(f"Orphaned joint detected during map build: {jnt_name}")

            self._joint_agent_map[j] = (agent_name, jnt_name)
            agent_names_found.add(agent_name)

        logger.info(
            f"joint_to_agent map built: {len(self._joint_agent_map)} actuated joints "
            f"across {len(agent_names_found)} agent(s): {sorted(agent_names_found)}"
        )

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
        # Uses the pre-built _joint_agent_map (built once in build_joint_agent_map)
        # to avoid kinematic tree traversal at 60Hz.
        for j, (agent_name, jnt_name) in self._joint_agent_map.items():
            jnt_type = model.jnt_type[j]
            val = float(data.qpos[model.jnt_qposadr[j]])
            state_key = f"jnt_{jnt_name}"

            # Only publish if the joint actually moved
            if state_key not in self.last_state or abs(val - self.last_state[state_key]) >= 0.001:
                if agent_name not in agent_payloads:
                    agent_payloads[agent_name] = {"joints": {"hinge": {}, "slide": {}}}

                # Flip hinge rotations for UE5's Left-Handed coordinate system. Slide joints are unaffected.
                if jnt_type == mujoco.mjtJoint.mjJNT_HINGE:
                    agent_payloads[agent_name]["joints"]["hinge"][jnt_name] = -val  # Left-Handed Flip
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
                # Dynamically create state publisher and ACK subscriber for each new agent.
                state_topic = f"sim/agent/{agent_name}/state"
                ack_topic = f"{ACK_TOPIC_PREFIX}/{agent_name}"
                self.agent_pubs[agent_name] = self.session.declare_publisher(state_topic)
                self._rtt_stats[agent_name] = {"samples": []}
                self._ack_subs[agent_name] = self.session.declare_subscriber(ack_topic, self._make_ack_handler(agent_name))
                logger.info(f"Opened dedicated Zenoh channels: {state_topic} | ACK: {ack_topic}")
            try:
                payload["_msg_id"] = self._msg_counter
                self._pending_acks[self._msg_counter] = time.time_ns()
                self._msg_counter += 1
                self.agent_pubs[agent_name].put(json.dumps(payload))
            except Exception as e:
                logger.error(f"Failed to encode/publish agent payload for {agent_name}: {e}")

    def _make_ack_handler(self, agent_name):
        """Returns a Zenoh callback that computes RTT for a specific agent."""
        def on_ack(sample):
            try:
                msg_id = int(sample.payload.to_string())
            except ValueError:
                logger.warning(f"[LATENCY/{agent_name}] Malformed ACK payload: '{sample.payload.to_string()}'")
                return

            send_ns = self._pending_acks.pop(msg_id, None)
            if send_ns is None:
                logger.warning(f"[LATENCY/{agent_name}] ACK for unknown msg_id={msg_id}. May have timed out or arrived late.")
                return

            rtt_ms = (time.time_ns() - send_ns) / 1e6
            self._rtt_stats[agent_name]["samples"].append(rtt_ms)

            samples = self._rtt_stats[agent_name]["samples"]
            if len(samples) >= RTT_LOG_WINDOW:
                avg = sum(samples) / len(samples)
                mn = min(samples)
                mx = max(samples)
                variance = sum((x - avg) ** 2 for x in samples) / len(samples)
                stddev = variance ** 0.5
                logger.info(
                    f"[LATENCY/{agent_name}] {len(samples)} samples | "
                    f"min={mn:.2f}ms avg={avg:.2f}ms max={mx:.2f}ms stddev={stddev:.2f}ms"
                )
                if mx > 10.0:
                    logger.warning(f"[LATENCY] REQ-B-01 VIOLATION: {agent_name} max RTT {mx:.2f}ms exceeds 10ms threshold.")
                self._rtt_stats[agent_name]["samples"] = []
        return on_ack

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
        
        # Always dump the stitched XML before attempting to load — this way it's
        # available for inspection even when MuJoCo's compiler rejects it.
        try:
            windows_debug_xml = xml_str.replace('/app/backend/assets/', 'assets/').replace('/app/assets/', 'assets/')
            with open(DEBUG_XML_PATH, "w") as f:
                f.write(windows_debug_xml)
            logger.info(f"Debug XML saved to {DEBUG_XML_PATH} ({len(xml_str)} chars). Inspect this file if model load fails.")
        except Exception as dump_err:
            logger.warning(f"Could not write debug XML to disk: {dump_err}")

        try:
            physics_model = mujoco.MjModel.from_xml_string(xml_str)
            physics_data = mujoco.MjData(physics_model)
            logger.info(
                f"SUCCESS! MuJoCo Physics Engine is Online. "
                f"Bodies={physics_model.nbody} Joints={physics_model.njnt} "
                f"Actuators={physics_model.nu} Meshes={physics_model.nmesh}"
            )
            state_pub.build_joint_agent_map(physics_model)
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
            
            # Step physics N times per render frame.
            # With a large joint count (60+ DOFs), 8 substeps × 60Hz = 480 steps/sec which is
            # expensive. Reduce to 2 for complex scenes while keeping the 60Hz publish rate.
            # Increase back to 8 only for scenes with fast dynamics or contact-heavy objects.
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

                # Prune ACKs that never arrived (5 second timeout) to prevent memory leak
                cutoff_ns = time.time_ns() - int(5e9)
                stale_ids = [k for k, v in state_pub._pending_acks.items() if v < cutoff_ns]
                for k in stale_ids:
                    state_pub._pending_acks.pop(k, None)
                    logger.warning(f"[LATENCY] ACK timeout for msg_id={k}. Payload may have been lost in transit.")
            
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