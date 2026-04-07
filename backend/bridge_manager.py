import zenoh
import orjson
import time
import mujoco
import numpy as np
import logging

logger = logging.getLogger("Bridge")

class BridgeManager:
    def __init__(self, session, telemetry):
        self.session = session
        self.telemetry = telemetry
        self.world_pub = self.session.declare_publisher("sim/world/state")

        # Low-Latency Configuration
        self.ACK_TOPIC_PREFIX = "sim/latency/ack"
        self.TEST_GHOST_PAYLOAD = False  # Toggle for pure network latency isolation

        # State tracking
        self.agent_pubs = {}
        self.last_state = {}
        self._msg_counter = 0
        self._ack_subs = {}

        # Joint Manifest: {jnt_index: agent_name}
        self._joint_agent_manifest = {}
        self._mobile_agents = {} # {agent_name: root_body_id}
        self._prop_ids = {}      # {prop_name: body_id}

        # Vectorized constants
        self.UE_POS_CONVERSION = np.array([100.0, -100.0, 100.0], dtype=np.float32)
        self.UE_QUAT_CONVERSION = np.array([-1.0, 1.0, -1.0, 1.0], dtype=np.float32)

        # Delta thresholds to prevent network spam
        self.POS_TOLERANCE = 0.001
        self.QUAT_TOLERANCE = 0.01

        # Subscribe to world ACKs immediately
        self._ack_subs["world"] = self.session.declare_subscriber(
            f"{self.ACK_TOPIC_PREFIX}/world",
            self._make_ack_handler("world")
        )

    def build_joint_manifest(self, model):
        """One-time scan of the model to map all joints to their parent agent bodies."""
        logger.info("Building Joint Manifest and Body ID cache...")
        self._joint_agent_manifest.clear()
        self._mobile_agents.clear()

        # 1. Identify Mobile Agents and their root bodies
        for j in range(model.njnt):
            if model.jnt_type[j] == mujoco.mjtJoint.mjJNT_FREE:
                body_id = model.jnt_bodyid[j]
                b_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_BODY, body_id)
                if b_name and b_name.endswith("__root"):
                    agent_name = b_name.replace("__root", "")
                    self._mobile_agents[agent_name] = body_id

        # 2. Map Joints to Agents via hierarchy traversal
        for j in range(model.njnt):
            jnt_type = model.jnt_type[j]
            if jnt_type not in [mujoco.mjtJoint.mjJNT_HINGE, mujoco.mjtJoint.mjJNT_SLIDE]:
                continue

            body_id = model.jnt_bodyid[j]
            agent_name = None

            # Traverse up the tree to find the __root body
            while body_id != 0:
                b_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_BODY, body_id)
                if b_name and b_name.endswith("__root"):
                    agent_name = b_name.replace("__root", "")
                    break
                body_id = model.body_parentid[body_id]

            if agent_name:
                self._joint_agent_manifest[j] = agent_name
            else:
                jnt_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_JOINT, j)
                logger.warning(f"Joint '{jnt_name}' is not child of any Agent root. It will not be published.") 

        logger.info(f"Manifest complete. Tracked {len(self._joint_agent_manifest)} joints across agents.")      

    def publish_state(self, model, data, dynamic_props):
        """Extracts physics state and pushes to Zenoh (Envelope Split Architecture)."""
        # Ensure prop ID cache is warm
        if not self._prop_ids and dynamic_props:
            for name in dynamic_props:
                self._prop_ids[name] = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_BODY, name)

        world_payload = {"objects": []}
        agent_payloads = {}
        has_world_updates = False

        # 1. PROPS (Optimized)
        for name, b_id in self._prop_ids.items():
            mj_pos = data.xpos[b_id]
            mj_quat = data.xquat[b_id]

            # Vectorized coordinate transform: RH -> LH, Meters -> CM
            ue_pos = mj_pos * self.UE_POS_CONVERSION
            # [w, x, y, z] -> [x, y, z, w] reordering then LH flip [-x, y, -z, w]
            ue_quat = mj_quat[[1, 2, 3, 0]] * self.UE_QUAT_CONVERSION

            if self._check_movement(name, ue_pos, ue_quat):
                world_payload["objects"].append({"name": name, "pos": ue_pos, "quat": ue_quat})
                self.last_state[name] = {'pos': ue_pos, 'quat': ue_quat}
                has_world_updates = True

        # 2. AGENT ROOTS
        for agent_name, b_id in self._mobile_agents.items():
            mj_pos = data.xpos[b_id]
            mj_quat = data.xquat[b_id]

            # Vectorized coordinate transform
            ue_pos = mj_pos * self.UE_POS_CONVERSION
            ue_quat = mj_quat[[1, 2, 3, 0]] * self.UE_QUAT_CONVERSION

            root_body_name = f"{agent_name}__root"
            if self._check_movement(root_body_name, ue_pos, ue_quat):
                if agent_name not in agent_payloads:
                    agent_payloads[agent_name] = {"joints": {"hinge": {}, "slide": {}}}
                agent_payloads[agent_name]["root_transform"] = {"pos": ue_pos, "quat": ue_quat}
                self.last_state[root_body_name] = {'pos': ue_pos, 'quat': ue_quat}

        # 3. JOINTS (Using Manifest)
        for j, agent_name in self._joint_agent_manifest.items():
            jnt_name_full = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_JOINT, j)
            val = float(data.qpos[model.jnt_qposadr[j]])
            state_key = f"jnt_{jnt_name_full}"

            # Only publish if delta > threshold
            if state_key not in self.last_state or abs(val - self.last_state[state_key]) >= 0.001:
                if agent_name not in agent_payloads:
                    agent_payloads[agent_name] = {"joints": {"hinge": {}, "slide": {}}}

                # Strip the agent prefix from the joint name
                prefix = f"{agent_name}_"
                jnt_name = jnt_name_full[len(prefix):] if jnt_name_full.startswith(prefix) else jnt_name_full   

                jnt_type = model.jnt_type[j]
                if jnt_type == mujoco.mjtJoint.mjJNT_HINGE:
                    agent_payloads[agent_name]["joints"]["hinge"][jnt_name] = -val
                else:
                    agent_payloads[agent_name]["joints"]["slide"][jnt_name] = val

                self.last_state[state_key] = val

        # 4. TRANSMIT (Envelope Split Architecture with orjson)
        if has_world_updates:
            # orjson.dumps returns bytes. OPT_SERIALIZE_NUMPY handles any residual numpy types.
            bin_payload = b"{}" if self.TEST_GHOST_PAYLOAD else orjson.dumps(world_payload, option=orjson.OPT_SERIALIZE_NUMPY)
            ack_topic = f"{self.ACK_TOPIC_PREFIX}/world"
            envelope = f"{ack_topic}:{self._msg_counter}|".encode() + bin_payload

            self.telemetry.register_message(self._msg_counter, "world")
            self._msg_counter += 1
            self.world_pub.put(envelope)

        for agent_name, payload in agent_payloads.items():
            if agent_name not in self.agent_pubs:
                self._setup_agent_channel(agent_name)

            bin_payload = b"{}" if self.TEST_GHOST_PAYLOAD else orjson.dumps(payload, option=orjson.OPT_SERIALIZE_NUMPY)
            ack_topic = f"{self.ACK_TOPIC_PREFIX}/{agent_name}"
            envelope = f"{ack_topic}:{self._msg_counter}|".encode() + bin_payload

            self.telemetry.register_message(self._msg_counter, agent_name)
            self._msg_counter += 1
            self.agent_pubs[agent_name].put(envelope)
    def _setup_agent_channel(self, agent_name):
        state_topic = f"sim/agent/{agent_name}/state"
        ack_topic = f"{self.ACK_TOPIC_PREFIX}/{agent_name}"
        
        self.agent_pubs[agent_name] = self.session.declare_publisher(state_topic)
        self._ack_subs[agent_name] = self.session.declare_subscriber(
            ack_topic, 
            self._make_ack_handler(agent_name)
        )
        logger.info(f"Initialized agent bridge: {agent_name}")

    def _make_ack_handler(self, agent_name):
        def on_ack(sample):
            try:
                msg_id = int(sample.payload.to_string())
                self.telemetry.handle_ack(agent_name, msg_id)
            except ValueError:
                pass
        return on_ack

    def _check_movement(self, name, new_pos, new_quat):
        if name not in self.last_state: return True
        old_pos = self.last_state[name]['pos']
        old_quat = self.last_state[name]['quat']
        if np.linalg.norm(np.array(new_pos) - np.array(old_pos)) > self.POS_TOLERANCE: return True
        if abs(np.dot(old_quat, new_quat)) < (1.0 - self.QUAT_TOLERANCE): return True
        return False
