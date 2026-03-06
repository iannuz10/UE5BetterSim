import os
import xml.etree.ElementTree as ET
import urdf_converter # Uses the safe, pristine converter you built in Step 1!

class WorldBuilder:
    def __init__(self, json_data):
        self.objects = json_data.get('objects', [])
        self.dynamic_props = []
        self.inserted_assets = set()

    def build(self):
        main_mujoco = ET.Element('mujoco', {'model': 'RoboSimUE_World'})
        ET.SubElement(main_mujoco, 'compiler', {'angle': 'radian'})
        ET.SubElement(main_mujoco, 'option', {'gravity': '0 0 -9.81'})
        
        main_asset = ET.SubElement(main_mujoco, 'asset')
        main_worldbody = ET.SubElement(main_mujoco, 'worldbody')
        
        ET.SubElement(main_worldbody, 'light', {'diffuse': '.5 .5 .5', 'pos': '0 0 10', 'dir': '0 0 -1'})
        ET.SubElement(main_asset, 'texture', {'type': 'skybox', 'builtin': 'gradient', 'rgb1': '0.3 0.5 0.7', 'rgb2': '0 0 0', 'width': '512', 'height': '512'})
        ET.SubElement(main_asset, 'texture', {'name': 'grid', 'type': '2d', 'builtin': 'checker', 'rgb1': '0.2 0.3 0.4', 'rgb2': '0.1 0.2 0.3', 'width': '512', 'height': '512'})
        ET.SubElement(main_asset, 'material', {'name': 'grid_mat', 'texture': 'grid', 'texrepeat': '1 1', 'texuniform': 'true', 'reflectance': '0.2'})

        for obj in self.objects:
            name = obj.get('name', 'Unknown')
            mesh_type = obj.get('mesh', '').lower()
            
            p = obj.get('position', [0,0,0])
            q = obj.get('quat', [1,0,0,0])
            pos_str = f"{p[0]} {p[1]} {p[2]}"
            quat_str = f"{q[0]} {q[1]} {q[2]} {q[3]}"

            # --- AGENTS (Robots) ---
            if mesh_type == 'agent' and obj.get('file_path'):
                self._process_agent(obj, main_asset, main_worldbody, pos_str, quat_str)
                continue
            
            # --- STATIC ENVIRONMENT ---
            if mesh_type in ['plane', 'floor']:
                size = obj.get('size', [50, 50])
                ET.SubElement(main_worldbody, 'geom', {
                    'name': name, 'type': 'plane', 'pos': pos_str, 'quat': quat_str, 
                    'size': f"{size[0]} {size[1]} 0.1", 'material': 'grid_mat'
                })
                continue
                
            # --- PROPS ---
            is_static = obj.get('is_static', False)
            if not is_static:
                self.dynamic_props.append(name)
                
            prop_body = ET.SubElement(main_worldbody, 'body', {'name': name, 'pos': pos_str, 'quat': quat_str})
            
            if not is_static:
                ET.SubElement(prop_body, 'freejoint', {'name': f"{name}_joint"})
            
            if mesh_type == 'cube':
                size = obj.get('size', [0.5, 0.5, 0.5])
                ET.SubElement(prop_body, 'geom', {'type': 'box', 'size': f"{size[0]} {size[1]} {size[2]}", 'rgba': '0.8 0.2 0.2 1', 'mass': '1.0'})
            elif mesh_type == 'sphere':
                radius = obj.get('size', [0.5])[0]
                ET.SubElement(prop_body, 'geom', {'type': 'sphere', 'size': str(radius), 'rgba': '0.2 0.2 0.8 1', 'mass': '1.0'})
            elif mesh_type == 'custom':
                mesh_path = obj.get('file_path', '')
                if mesh_path:
                    abs_mesh = os.path.abspath(mesh_path)
                    mesh_name = f"{name}_mesh"
                    if mesh_name not in self.inserted_assets:
                        ET.SubElement(main_asset, 'mesh', {'name': mesh_name, 'file': abs_mesh})
                        self.inserted_assets.add(mesh_name)
                    ET.SubElement(prop_body, 'geom', {'type': 'mesh', 'mesh': mesh_name, 'rgba': '0.7 0.7 0.7 1', 'mass': '1.0'})

        xml_string = ET.tostring(main_mujoco, encoding='unicode')
        return xml_string, self.dynamic_props

    def _process_agent(self, obj, main_asset, main_worldbody, pos_str, quat_str):
        urdf_path = obj.get('file_path')
        name = obj.get('name')
        
        # Temporary heuristic: If the file path lacks 'kuka', treat it as mobile.
        is_mobile = obj.get('is_mobile', not ('kuka' in str(urdf_path).lower()))

        pristine_mjcf = urdf_converter.convert_urdf_to_mjcf(urdf_path)
        urdf_dir = os.path.dirname(os.path.abspath(urdf_path))

        agent_tree = ET.parse(pristine_mjcf)
        agent_root = agent_tree.getroot()

        # Merge Assets (Absolute Paths)
        agent_asset = agent_root.find('asset')
        if agent_asset is not None:
            for asset in list(agent_asset):
                if asset.tag == 'mesh':
                    mesh_file = asset.get('file')
                    if mesh_file and not os.path.isabs(mesh_file):
                        asset.set('file', os.path.join(urdf_dir, mesh_file))
                
                asset_id = asset.get('name', asset.get('file', 'unknown'))
                if asset_id not in self.inserted_assets:
                    main_asset.append(asset)
                    self.inserted_assets.add(asset_id)

        # THE ULTIMATE FIX: The Master Wrapper
        agent_worldbody = agent_root.find('worldbody')
        if agent_worldbody is not None:
            # We create an entirely new, empty body. 
            wrapper_body = ET.Element('body', {'name': f"{name}__root", 'pos': pos_str, 'quat': quat_str})
            
            # If mobile, give the WRAPPER the freejoint. It has exactly 6 DOFs, keeping MuJoCo happy!
            if is_mobile:
                ET.SubElement(wrapper_body, 'freejoint', {'name': f"{name}__freejoint"})

            # Scoop EVERY piece of the robot (including loose Kuka bases) and put them inside the wrapper
            for child in list(agent_worldbody):
                wrapper_body.append(child)
                
            main_worldbody.append(wrapper_body)
                
        print(f"[WorldBuilder] Stitched Agent '{name}' into memory. Mobile: {is_mobile}")