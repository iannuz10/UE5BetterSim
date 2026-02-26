import zenoh
import json
import time
import mujoco
import numpy as np

# Configuration Constants
ZENOH_ENDPOINT = "tcp/host.docker.internal:7447"
INIT_TOPIC = "sim/world/init"
STATE_TOPIC = "sim/world/state"
DEBUG_XML_PATH = "/app/debug_world.xml" 

# Thresholds: 1 millimeter for position, ~0.5 degrees for rotation
POS_TOLERANCE = 0.001 
QUAT_TOLERANCE = 0.01

def has_moved_visually(old_pos, new_pos, old_quat, new_quat):
    pos_diff = np.linalg.norm(np.array(new_pos) - np.array(old_pos))
    if pos_diff > POS_TOLERANCE:
        return True
        
    # Quick quaternion distance (1.0 means identical, 0.0 means 180 deg apart)
    quat_dot = abs(np.dot(old_quat, new_quat))
    if quat_dot < (1.0 - QUAT_TOLERANCE):
        return True
        
    return False

def generate_mujoco_xml(json_data):
    """Translates the Unreal JSON into MuJoCo MJCF XML format."""
    
    # Start the XML with standard physics parameters
    xml = ['<mujoco model="RoboSimUE_World">']
    xml.append('  <compiler angle="radian"/>')
    xml.append('  <option gravity="0 0 -9.81"/>') # Standard Earth gravity
    
    # Define materials and lighting for the visualizer
    xml.append('  <asset>')
    xml.append('    <texture type="skybox" builtin="gradient" rgb1="0.3 0.5 0.7" rgb2="0 0 0" width="512" height="512"/>')
    xml.append('    <texture name="grid" type="2d" builtin="checker" rgb1="0.2 0.3 0.4" rgb2="0.1 0.2 0.3" width="512" height="512"/>')
    xml.append('    <material name="grid_mat" texture="grid" texrepeat="1 1" texuniform="true" reflectance="0.2"/>')
    xml.append('  </asset>')
    
    xml.append('  <worldbody>')
    xml.append('    <light diffuse=".5 .5 .5" pos="0 0 10" dir="0 0 -1"/>')

    dynamic_objects = [] # Keep track of what we need to simulate

    # Loop through every object sent from Unreal
    for obj in json_data.get('objects', []):
        name = obj['name']
        mesh_type = obj['mesh'].lower()
        
        # Convert arrays to space-separated strings for XML
        pos_str = f"{obj['position'][0]} {obj['position'][1]} {obj['position'][2]}"
        quat_str = f"{obj['quat'][0]} {obj['quat'][1]} {obj['quat'][2]} {obj['quat'][3]}"
        
        # FLOOR (Static)
        if mesh_type == 'plane' or mesh_type == 'floor':
            # Plane size is half-x and half-y. 
            size_str = f"{obj['size'][0]} {obj['size'][1]} 0.1" 
            xml.append(f'    <!-- Floor -->')
            xml.append(f'    <geom name="{name}" type="plane" pos="{pos_str}" quat="{quat_str}" size="{size_str}" material="grid_mat"/>')
            
        # PHYSICS OBJECTS (Dynamic)
        else:
            dynamic_objects.append(name)

            xml.append(f'    <!-- {name} -->')
            # The body defines the center of mass and starting position
            xml.append(f'    <body name="{name}" pos="{pos_str}" quat="{quat_str}">')
            # freejoint makes it fall and react to collisions
            xml.append(f'      <freejoint name="{name}_joint"/>')
            
            if mesh_type == 'cube':
                size_str = f"{obj['size'][0]} {obj['size'][1]} {obj['size'][2]}"
                xml.append(f'      <geom type="box" size="{size_str}" rgba="0.8 0.2 0.2 1" mass="1.0"/>')
                
            elif mesh_type == 'sphere':
                # Sphere only takes ONE size parameter (radius)
                radius = obj['size'][0] 
                xml.append(f'      <geom type="sphere" size="{radius}" rgba="0.2 0.2 0.8 1" mass="1.0"/>')
                
            xml.append('    </body>')

    xml.append('  </worldbody>')
    xml.append('</mujoco>')
    
    return "\n".join(xml), dynamic_objects

def main():
    print("[Python] Starting MuJoCo Zenoh Listener...")
    
    # Connect to Unreal
    conf = zenoh.Config()
    conf.insert_json5("mode", "'client'")
    conf.insert_json5("connect/endpoints", f"['{ZENOH_ENDPOINT}']")
    session = zenoh.open(conf)
    
    # Create a publisher for sending the physics state back
    state_pub = session.declare_publisher(STATE_TOPIC)

    print(f"[Python] Connected! Waiting for world data on '{INIT_TOPIC}'...")

    # Flag to keep the loop running until world is loaded
    world_loaded = False
    physics_model = None
    physics_data = None
    dynamic_bodies = []

    def on_init_message(sample):
        nonlocal world_loaded, physics_model, physics_data, dynamic_bodies
        
        # Decode the JSON string sent from Unreal
        payload = sample.payload.to_string()
        json_data = json.loads(payload)
        print(f"\n[Python] Received World Data! ({len(json_data['objects'])} objects found)")
        
        # Generate XML
        xml_string, dynamic_bodies = generate_mujoco_xml(json_data)
        
        # Save to a file so it can be viewed in Windows
        with open(DEBUG_XML_PATH, "w") as f:
            f.write(xml_string)
        print(f"[Python] Saved debug XML to: {DEBUG_XML_PATH}")
        
        # Load the world into MuJoCo's physics engine
        try:
            physics_model = mujoco.MjModel.from_xml_string(xml_string)
            physics_data = mujoco.MjData(physics_model)
            print("[Python] SUCCESS! MuJoCo Physics Engine initialized and ready.")
            world_loaded = True
        except Exception as e:
            print(f"[Python] ERROR Loading MuJoCo: {e}")

    # Subscribe to the topic
    sub = session.declare_subscriber(INIT_TOPIC, on_init_message)

    # Keep script alive while waiting
    while not world_loaded:
        time.sleep(0.1)
        
    # Clean up subscriber since we only need the init message once
    sub.undeclare()

    # ==========================================
    # THE SIMULATION LOOP
    # ==========================================
    print("[Python] Starting Physics Simulation Loop! (Ctrl+C to stop).")
    
    # Target 60 FPS update rate to Unreal
    frame_time = 1.0 / 60.0 

    # Dict to track last sent state for each object
    last_sent_states = {}

    # # Debug and performance analysis
    # frame_counter = 0
    # skipped_objects_this_second = 0
    # total_objects_this_second = 0
    
    try:
        while True:
            step_start = time.perf_counter()
            
            # 1. Step the Physics Engine (MuJoCo default timestep is 0.002s)
            # We step it ~8 times to simulate ~0.016s of time (1/60th of a sec)
            for _ in range(8):
                mujoco.mj_step(physics_model, physics_data)
                
            # 2. Gather the new positions
            state_dict = {"objects": []}
            
            for name in dynamic_bodies:
                # total_objects_this_second += 1 # Track total possible updates

                # Get raw MuJoCo arrays
                mj_pos = physics_data.body(name).xpos
                mj_quat = physics_data.body(name).xquat # MuJoCo format: [w, x, y, z]

                # Check if this object has moved significantly since last sent state
                needs_update = True
                if name in last_sent_states:
                    old_pos, old_quat = last_sent_states[name]
                    if not has_moved_visually(old_pos, mj_pos, old_quat, mj_quat):
                        needs_update = False
                        # skipped_objects_this_second += 1 # Track savings

                if needs_update:
                    # Update last sent state
                    last_sent_states[name] = (mj_pos.copy(), mj_quat.copy())
                
                    # Math Conversion: Back to Unreal Format!
                    # Meters -> Centimeters, and Right-Hand -> Left-Hand (Invert Y)
                    ue_pos = [mj_pos[0] * 100.0, mj_pos[1] * -100.0, mj_pos[2] * 100.0]
                    
                    # Quat Conversion: [w, x, y, z] -> [-x, y, -z, w] (Revert our earlier inversion)
                    ue_quat = [-mj_quat[1], mj_quat[2], -mj_quat[3], mj_quat[0]]
                    
                    state_dict["objects"].append({
                        "name": name,
                        "pos": ue_pos,
                        "quat": ue_quat
                    })
                
            # 3. Publish back to Unreal
            if len(state_dict["objects"]) > 0:
                state_pub.put(json.dumps(state_dict))

            # # Print Debug Stats Once Per Second (approx 60 frames)
            # frame_counter += 1
            # if frame_counter >= 60:
            #     if total_objects_this_second > 0:
            #         percent_saved = (skipped_objects_this_second / total_objects_this_second) * 100
            #         print(f"[Debug] Delta Updates: Skipped {skipped_objects_this_second}/{total_objects_this_second} object updates ({percent_saved:.1f}% bandwidth saved this second).")
                
            #     # Reset counters for the next second
            #     frame_counter = 0
            #     skipped_objects_this_second = 0
            #     total_objects_this_second = 0
            
            # 4. Sleep to match Real-Time
            elapsed = time.perf_counter() - step_start
            sleep_time = frame_time - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)
                
    except KeyboardInterrupt:
        print("\n[Python] Simulation Stopped by User.")

if __name__ == "__main__":
    main()