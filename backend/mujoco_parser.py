import zenoh
import json
import time
import mujoco

# Configuration Constants
ZENOH_ENDPOINT = "tcp/host.docker.internal:7447"
INIT_TOPIC = "sim/world/init"
DEBUG_XML_PATH = "/app/debug_world.xml" 

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
    
    return "\n".join(xml)

def main():
    print("[Python] Starting MuJoCo Zenoh Listener...")
    
    # Connect to Unreal
    conf = zenoh.Config()
    conf.insert_json5("mode", "'client'")
    conf.insert_json5("connect/endpoints", f"['{ZENOH_ENDPOINT}']")
    session = zenoh.open(conf)
    
    print(f"[Python] Connected! Waiting for world data on '{INIT_TOPIC}'...")

    # Flag to keep the loop running until world is loaded
    world_loaded = False
    physics_model = None
    physics_data = None

    def on_init_message(sample):
        nonlocal world_loaded, physics_model, physics_data
        
        # Decode the JSON string sent from Unreal
        payload = sample.payload.to_string()
        json_data = json.loads(payload)
        print(f"\n[Python] Received World Data! ({len(json_data['objects'])} objects found)")
        
        # Generate XML
        xml_string = generate_mujoco_xml(json_data)
        
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
        
    print("[Python] Simulation Loop would start here!")
    # Clean up subscriber since we only need the init message once
    sub.undeclare()

if __name__ == "__main__":
    main()