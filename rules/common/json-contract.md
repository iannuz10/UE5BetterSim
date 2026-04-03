# Core Architecture: JSON Payload Contract
   
All payloads transmitted over the Zenoh Bridge must strictly adhere to this schema. The Guest (Python) operates on a
"Fail-Fast" principle: missing keys will result in an immediate `ValueError` and simulation halt. Silent fallbacks are
strictly prohibited.

## 1. Initialization Payload
**Topic:** `sim/world/init`
**Direction:** Host (UE5) -> Guest (Python)
**Trigger:** Sent once at startup when 'P' is pressed.
{
  "objects": [
    {
      "name": "string (Unique ID, e.g., 'StaticMeshActor_1')",
      "mesh": "string (ENUM: 'agent', 'custom', 'cube', 'sphere', 'plane')",
      "is_static": "boolean (true if static prop, false if physics prop)",
      "position": ["float (X)", "float (Y)", "float (Z)"],
      "quat": ["float (W)", "float (X)", "float (Y)", "float (Z)"],

      // --- CONDITIONAL FIELDS based on "mesh" type ---

      // If mesh == 'agent'
      "file_path": "string (Path to robot XML, e.g., 'assets/robots/kuka.xml')",
      "is_mobile": "boolean (true if freejoint needed, false if bolted)",

      // If mesh == 'custom'
      "file_path": "string (Path to environment OBJ, e.g., 'assets/environment/ramp.obj')",
      "scale": ["float (X)", "float (Y)", "float (Z)"], // Raw UE5 scale multiplier

      // If mesh == 'cube', 'sphere', or 'plane'
      "size": ["float (Half-extents in meters)"] // e.g., [1.0, 1.0, 0.5] for cube
    }
  ]
}

  
## 2. World State Payload
**Topic:** `sim/world/state`
**Direction:** Guest (Python) -> Host (UE5)
**Trigger:** Sent every 60Hz frame (Delta compressed: only moving objects are included).
{
  "_msg_id": "integer (Monotonically increasing ID for RTT tracking)",
  "_timestamp": "integer (Send time in nanoseconds)",
  "objects": [
    {
      "name": "string (Must match the Init name)",
      "pos": ["float (X cm)", "float (Y cm)", "float (Z cm)"],
      "quat": ["float (X)", "float (Y)", "float (Z)", "float (W)"]
    }
  ]
}
  
## 3. Agent Joint Update Payload
**Topic:** `sim/agent/{agent_name}/state`
**Direction:** Guest (Python) -> Host (UE5)
**Trigger:** Sent every 60Hz frame per agent (Delta compressed).
{
  "_msg_id": "integer (Monotonically increasing ID for RTT tracking)",
  "_timestamp": "integer (Send time in nanoseconds)",
  "root_transform": {
    "pos": ["float (X cm)", "float (Y cm)", "float (Z cm)"],
    "quat": ["float (X)", "float (Y)", "float (Z)", "float (W)"]
  }, // Optional: Only present if agent is mobile
  "joints": {
    "hinge": {
      "joint_name_1": "float (Radians, Left-Handed Flipped)",
      "joint_name_2": "float (Radians, Left-Handed Flipped)"
    },
    "slide": {
      "prismatic_joint_1": "float (Meters)"
    }
  }
}