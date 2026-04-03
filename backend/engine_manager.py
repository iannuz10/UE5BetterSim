import mujoco
import logging
import os

logger = logging.getLogger("Physics")

class PhysicsEngine:
    def __init__(self, debug_xml_path="/app/debug_world.xml"):
        self.model = None
        self.data = None
        self.debug_xml_path = debug_xml_path
        self._is_ready = False

    def load_model(self, xml_str):
        """Compiles the MuJoCo XML and initializes data structures."""
        # Dump debug XML for inspection
        self._dump_debug_xml(xml_str)
        
        try:
            self.model = mujoco.MjModel.from_xml_string(xml_str)
            self.data = mujoco.MjData(self.model)
            logger.info(
                f"MuJoCo Loaded: Bodies={self.model.nbody} Joints={self.model.njnt} "
                f"Actuators={self.model.nu}"
            )
            self._is_ready = True
            return True
        except Exception as e:
            logger.critical(f"MuJoCo Compile Error: {e}")
            return False

    def step(self, substeps=8):
        """Advances the simulation by N substeps."""
        if not self._is_ready:
            return
        
        for _ in range(substeps):
            mujoco.mj_step(self.model, self.data)

    def is_ready(self):
        return self._is_ready

    def _dump_debug_xml(self, xml_str):
        """Saves a debug version of the stitched XML to disk."""
        try:
            # Clean up paths for Windows-side inspection if needed
            clean_xml = xml_str.replace('/app/backend/assets/', 'assets/').replace('/app/assets/', 'assets/')
            with open(self.debug_xml_path, "w") as f:
                f.write(clean_xml)
            logger.info(f"Debug XML saved to {self.debug_xml_path}")
        except Exception as e:
            logger.warning(f"Could not dump debug XML: {e}")
