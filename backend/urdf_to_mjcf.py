import os
import re
import mujoco
import xml.etree.ElementTree as ET

def process_agent(file_path, name, pos, quat, is_mobile):
    abs_file = os.path.abspath(file_path)
    base_dir = os.path.dirname(abs_file)
    base_mjcf = abs_file
    
    # 1. COMPILE URDF SAFELY 
    if abs_file.endswith('.urdf'):
        base_mjcf = abs_file.replace('.urdf', '.mjcf.xml')
        
        # Read original URDF
        with open(abs_file, 'r') as f:
            urdf_text = f.read()
            
        # Strip old mujoco blocks and inject a pristine one
        urdf_text = re.sub(r'<mujoco>.*?</mujoco>', '', urdf_text, flags=re.DOTALL)
        pristine_block = '\n  <mujoco>\n    <compiler strippath="false" balanceinertia="true" discardvisual="false"/>\n  </mujoco>\n'
        urdf_text = re.sub(r'(<robot[^>]*>)', r'\1' + pristine_block, urdf_text, count=1)
        
        # Save to a HIDDEN temp file, compile, and delete temp. Original URDF is never altered
        temp_urdf = abs_file.replace('.urdf', '_temp.urdf')
        with open(temp_urdf, 'w') as f:
            f.write(urdf_text)
            
        orig_cwd = os.getcwd()
        try:
            os.chdir(base_dir) # Shift context so meshes are found
            model = mujoco.MjModel.from_xml_path(temp_urdf)
            mujoco.mj_saveLastXML(base_mjcf, model)
        finally:
            os.chdir(orig_cwd)
            if os.path.exists(temp_urdf):
                os.remove(temp_urdf)

    # 2. UNIVERSAL SPAWN ANCHORING
    tree = ET.parse(base_mjcf)
    root = tree.getroot()
    worldbody = root.find('worldbody')
    
    clean_name = name.replace(":", "_")
    instance_path = os.path.join(base_dir, f"{clean_name}_instance.xml")
    
    if worldbody is not None:
        # Create a master wrapper using the exact UE5 spawn coordinates
        agent_root = ET.Element('body', {
            'name': f"{name}__root", 
            'pos': f"{pos[0]} {pos[1]} {pos[2]}", 
            'quat': f"{quat[0]} {quat[1]} {quat[2]} {quat[3]}"
        })
        
        # If UE5 said it's a drone/humanoid/rover, give it a freejoint so it falls and moves!
        if is_mobile:
            ET.SubElement(agent_root, 'freejoint', {'name': f"{name}_freejoint"})
            
        # Move all robot parts inside the wrapper
        for child in list(worldbody):
            agent_root.append(child)
            worldbody.remove(child)
            
        worldbody.append(agent_root)
        tree.write(instance_path)
        print(f"[Converter] Built Agent '{name}'. Mobile: {is_mobile}")
        
    return instance_path