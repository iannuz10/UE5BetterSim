import zenoh
import json
import time
import logging
from world_builder import WorldBuilder
from telemetry_manager import TelemetryManager
from bridge_manager import BridgeManager
from engine_manager import PhysicsEngine

# ==========================================
# CONFIGURATION
# ==========================================
ZENOH_ENDPOINT = "tcp/host.docker.internal:7447"
INIT_TOPIC = "sim/world/init"
STOP_TOPIC = "sim/control/stop"
FPS = 60

# Telemetry setup
logging.basicConfig(
    filename='mujoco_run.log', 
    filemode='w', 
    level=logging.DEBUG, 
    format='[%(asctime)s] [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger("MuJoCo_Core")

class SimulationController:
    def __init__(self):
        self.telemetry = TelemetryManager(threshold_ms=4.0)
        self.engine = PhysicsEngine()
        self.bridge = None
        self.dynamic_props = []
        self._is_ready = False
        self._is_running = True

    def connect_zenoh(self, endpoint):
        logger.info(f"Connecting to Zenoh at {endpoint}...")
        try:
            conf = zenoh.Config()
            conf.insert_json5("mode", "'client'")
            conf.insert_json5("connect/endpoints", f"['{endpoint}']")
            session = zenoh.open(conf)
            self.bridge = BridgeManager(session, self.telemetry)
            return True
        except Exception as e:
            logger.critical(f"Failed to connect to Zenoh: {e}")
            return False

    def on_init(self, sample):
        """Zenoh callback for world initialization."""
        logger.info("Received INIT payload from Unreal Engine.")
        try:
            payload = json.loads(sample.payload.to_string())
        except json.JSONDecodeError as e:
            logger.critical(f"Malformed INIT JSON: {e}")
            return

        builder = WorldBuilder(payload)
        xml_str, self.dynamic_props = builder.build()

        if self.engine.load_model(xml_str):
            self.bridge.build_joint_manifest(self.engine.model)
            self._is_ready = True

    def on_stop(self, sample):
        """Zenoh callback for simulation stop."""
        logger.info("Received STOP signal from Unreal Engine. Shutting down...")
        self._is_running = False

    def run(self):
        """Main simulation loop."""
        if not self.connect_zenoh(ZENOH_ENDPOINT):
            return

        # Wait for initialization
        sub = self.bridge.session.declare_subscriber(INIT_TOPIC, self.on_init)
        logger.info(f"Waiting for '{INIT_TOPIC}'...")
        while not self._is_ready:
            time.sleep(0.1)
        sub.undeclare()

        # Listen for stop signal
        stop_sub = self.bridge.session.declare_subscriber(STOP_TOPIC, self.on_stop)

        # Calculate optimal substeps once model is loaded
        substeps = self.engine.get_optimal_substeps(FPS)

        logger.info(f"Starting physics loop at {FPS}Hz (substeps={substeps})...")
        frame_time = 1.0 / FPS
        frame_count = 0

        try:
            while self._is_running:
                start_tick = time.perf_counter()

                # 1. Physics
                self.engine.step(substeps=substeps)

                # 2. Bridge
                self.bridge.publish_state(self.engine.model, self.engine.data, self.dynamic_props)

                # 3. Telemetry & Cleanup (Every 1 second at 60Hz)
                frame_count += 1
                if frame_count % FPS == 0:
                    self.telemetry.check_anomalies(self.engine.model, self.engine.data)
                    self.telemetry.prune_stale_acks()

                # 4. Lockstep frequency
                elapsed = time.perf_counter() - start_tick
                if elapsed < frame_time:
                    time.sleep(frame_time - elapsed)
        except KeyboardInterrupt:
            logger.info("Simulation stopped by user.")
        except Exception as e:
            logger.critical(f"Runtime crash: {e}")
        finally:
            if 'stop_sub' in locals():
                stop_sub.undeclare()
            
            if self.bridge and self.bridge.session:
                logger.info("Closing Zenoh session...")
                self.bridge.session.close()

            logger.info("Simulation loop exited cleanly.")

if __name__ == "__main__":
    controller = SimulationController()
    controller.run()
