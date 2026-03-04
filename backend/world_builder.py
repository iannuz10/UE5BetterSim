import urdf_to_mjcf
import os

def build_world(json_data):
    xml = ['<mujoco model="RoboSimUE_World">']
    xml.extend(['  <compiler angle="radian"/>', '  <option gravity="0 0 -9.81"/>'])
    
    dynamic_props = []
    
    # 1. Process All Agents (Universally)
    for obj in json_data.get('objects', []):
        if obj.get('mesh', '').lower() == 'agent' and obj.get('file_path'):
            is_mobile = obj.get('is_mobile', False)
            instance_xml = urdf_to_mjcf.process_agent(obj['file_path'], obj['name'], obj['position'], obj['quat'], is_mobile)
            
            # Use absolute paths for bulletproof Docker loading
            xml.append(f'  <include file="{os.path.abspath(instance_xml)}"/>')
            
    # 2. Process Assets
    xml.append('  <asset>')
    xml.append('    <texture type="skybox" builtin="gradient" rgb1="0.3 0.5 0.7" rgb2="0 0 0" width="512" height="512"/>')
    xml.append('    <texture name="grid" type="2d" builtin="checker" rgb1="0.2 0.3 0.4" rgb2="0.1 0.2 0.3" width="512" height="512"/>')
    xml.append('    <material name="grid_mat" texture="grid" texrepeat="1 1" texuniform="true" reflectance="0.2"/>')
    
    for obj in json_data.get('objects', []):
        if obj.get('mesh', '').lower() == 'custom' and obj.get('file_path'):
            xml.append(f'    <mesh name="{obj["name"]}_mesh" file="{os.path.abspath(obj["file_path"])}"/>')
    xml.append('  </asset>')
    
    # 3. Process Dynamic Props & Floors
    xml.append('  <worldbody>')
    xml.append('    <light diffuse=".5 .5 .5" pos="0 0 10" dir="0 0 -1"/>')

    for obj in json_data.get('objects', []):
        name, mesh_type = obj['name'], obj['mesh'].lower()
        if mesh_type == 'agent': continue
            
        p, q = obj['position'], obj['quat']
        pos_str, quat_str = f"{p[0]} {p[1]} {p[2]}", f"{q[0]} {q[1]} {q[2]} {q[3]}"
        
        if mesh_type in ['plane', 'floor']:
            xml.append(f'    <geom name="{name}" type="plane" pos="{pos_str}" quat="{quat_str}" size="{obj["size"][0]} {obj["size"][1]} 0.1" material="grid_mat"/>')
        else:
            dynamic_props.append(name) 
            xml.append(f'    <body name="{name}" pos="{pos_str}" quat="{quat_str}">\n      <freejoint name="{name}_joint"/>')
            if mesh_type == 'cube':
                xml.append(f'      <geom type="box" size="{obj["size"][0]} {obj["size"][1]} {obj["size"][2]}" rgba="0.8 0.2 0.2 1" mass="1.0"/>')
            elif mesh_type == 'sphere':
                xml.append(f'      <geom type="sphere" size="{obj["size"][0]}" rgba="0.2 0.2 0.8 1" mass="1.0"/>')
            elif mesh_type == 'custom':
                xml.append(f'      <geom type="mesh" mesh="{name}_mesh" rgba="0.7 0.7 0.7 1" mass="1.0"/>')
            xml.append('    </body>')

    xml.extend(['  </worldbody>', '</mujoco>'])
    return "\n".join(xml), dynamic_props