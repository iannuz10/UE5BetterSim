import time
import logging
import numpy as np
import mujoco

logger = logging.getLogger("Telemetry")

class TelemetryManager:
    def __init__(self, rtt_window=60, threshold_ms=3.0):
        self.rtt_window = rtt_window
        self.threshold_ms = threshold_ms
        self._pending_acks = {}  # {msg_id: (send_time_ns, agent_name)}
        self._rtt_stats = {}     # {agent_name: {"samples": []}}
        self._fallen_objects = set()
        
    def register_message(self, msg_id, agent_name):
        """Records the send time and owner of a message."""
        self._pending_acks[msg_id] = (time.time_ns(), agent_name)

    def handle_ack(self, agent_name, msg_id):
        """Computes RTT. Validates that the ACK matches the expected agent."""
        data = self._pending_acks.pop(msg_id, None)
        if data is None:
            return

        send_ns, original_agent = data
        
        # Cross-check: Did the correct agent respond?
        if original_agent != agent_name:
            logger.warning(f"ACK CROSS-TALK: Agent '{agent_name}' responded to MsgID {msg_id} owned by '{original_agent}'!")

        rtt_ms = (time.time_ns() - send_ns) / 1e6
        
        if agent_name not in self._rtt_stats:
            self._rtt_stats[agent_name] = {"samples": []}
            
        samples = self._rtt_stats[agent_name]["samples"]
        samples.append(rtt_ms)

        if len(samples) >= self.rtt_window:
            self._log_rtt_report(agent_name, samples)
            self._rtt_stats[agent_name]["samples"] = []

    def _log_rtt_report(self, agent_name, samples):
        avg = sum(samples) / len(samples)
        mx = max(samples)
        stddev = np.std(samples)
        
        logger.info(
            f"[RTT/{agent_name}] Avg: {avg:.2f}ms | Max: {mx:.2f}ms | StdDev: {stddev:.2f}ms ({len(samples)} samples)"
        )
        
        if mx > self.threshold_ms:
            logger.warning(f"LATENCY VIOLATION: {agent_name} spiked to {mx:.2f}ms (Target: <{self.threshold_ms}ms)")

    def prune_stale_acks(self, timeout_sec=2.0):
        """Aggregates timeouts and logs a summary instead of individual messages."""
        cutoff_ns = time.time_ns() - int(timeout_sec * 1e9)
        stale_ids = [k for k, v in self._pending_acks.items() if v[0] < cutoff_ns]
        
        if not stale_ids:
            return

        current_timeouts = {}
        for k in stale_ids:
            _, agent_name = self._pending_acks.pop(k)
            current_timeouts[agent_name] = current_timeouts.get(agent_name, 0) + 1
        
        for agent, count in current_timeouts.items():
            logger.error(f"[NETWORK/{agent}] LOST {count} ACKs in the last window. Potential lag or UE5 hang.")

    def check_anomalies(self, model, data):
        """Standard physics stability checks."""
        if np.any(np.abs(data.qvel) > 100.0) or np.any(np.isnan(data.qpos)):
            logger.error("PHYSICS ANOMALY: Instability detected (Explosion or NaN).")

        for i in range(1, model.nbody):
            if data.xpos[i][2] < -20.0:
                b_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_BODY, i)
                if b_name and b_name not in self._fallen_objects:
                    logger.warning(f"PHYSICS ANOMALY: Body '{b_name}' fell out of bounds.")
                    self._fallen_objects.add(b_name)
