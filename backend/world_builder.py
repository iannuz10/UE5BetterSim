import os
import xml.etree.ElementTree as ET

class WorldBuilder:
    def __init__(self, json_data):
        self.objects = json_data.get('objects', [])
        self.dynamic_props = []
        self.inserted_assets = set()
        self.inserted_physics_elements = set() 

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
                self._process_agent(obj, main_mujoco, main_asset, main_worldbody, pos_str, quat_str)
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
                ET.SubElement(prop_body, 'geom', {'type': 'box', 'size': f"{size[0]} {size[1]} {size[2]}", 'rgba': '0.8 0.2 0.2 1', 'mass': '50.0'})
            elif mesh_type == 'sphere':
                radius = obj.get('size', [0.5])[0]
                ET.SubElement(prop_body, 'geom', {'type': 'sphere', 'size': str(radius), 'rgba': '0.2 0.2 0.8 1', 'mass': '1.0'})
            elif mesh_type == 'custom':
                mesh_path = obj.get('file_path', '')
                if mesh_path:
                    abs_mesh = os.path.abspath(mesh_path)
                    mesh_name = f"{name}_mesh"
                    
                    # 1. PURE SCALE (No negative signs!)
                    ue5_scale = obj.get('scale', [1.0, 1.0, 1.0])
                    mj_scale = f"{ue5_scale[0] * 0.01} {ue5_scale[1] * 0.01} {ue5_scale[2] * 0.01}"

                    if mesh_name not in self.inserted_assets:
                        ET.SubElement(main_asset, 'mesh', {'name': mesh_name, 'file': abs_mesh, 'scale': mj_scale})
                        self.inserted_assets.add(mesh_name)
                    
                    # 2. PURE GEOM (No quat overrides!)
                    ET.SubElement(prop_body, 'geom', {
                        'type': 'mesh', 
                        'mesh': mesh_name, 
                        'rgba': '0.7 0.7 0.7 1', 
                        'mass': '1.0'
                    })

        xml_string = ET.tostring(main_mujoco, encoding='unicode')
        return xml_string, self.dynamic_props

    def _process_agent(self, obj, main_mujoco, main_asset, main_worldbody, pos_str, quat_str):
        mjcf_path = obj.get('file_path')
        name = obj.get('name')
        
        is_mobile = obj.get('is_mobile', not ('kuka' in str(mjcf_path).lower()))

        abs_mjcf_path = os.path.abspath(mjcf_path)
        mjcf_dir = os.path.dirname(abs_mjcf_path)

        try:
            agent_tree = ET.parse(abs_mjcf_path)
        except Exception as e:
            print(f"[WorldBuilder] ERROR: Could not parse {abs_mjcf_path}. Error: {e}")
            return
            
        agent_root = agent_tree.getroot()

        # 1. Merge Compiler & Options (THE TRUE NaN FIX FOR MENAGERIE)
        agent_compiler = agent_root.find('compiler')
        if agent_compiler is not None:
            main_compiler = main_mujoco.find('compiler')
            for key, val in agent_compiler.attrib.items():
                if key not in ['meshdir', 'texturedir', 'angle']: 
                    main_compiler.set(key, val)
            meshdir = agent_compiler.get('meshdir', '')
            texturedir = agent_compiler.get('texturedir', meshdir)
        else:
            meshdir = ''
            texturedir = ''

        agent_option = agent_root.find('option')
        if agent_option is not None:
            main_option = main_mujoco.find('option')
            for key, val in agent_option.attrib.items():
                if key != 'gravity': # Respect the master world's gravity
                    main_option.set(key, val)

        # 2. Merge Assets (Absolute Paths)
        agent_asset = agent_root.find('asset')
        if agent_asset is not None:
            for asset in list(agent_asset):
                if asset.tag in ['mesh', 'texture']:
                    mesh_file = asset.get('file')
                    if mesh_file and not os.path.isabs(mesh_file):
                        target_dir = texturedir if asset.tag == 'texture' else meshdir
                        full_path = os.path.normpath(os.path.join(mjcf_dir, target_dir, mesh_file))
                        asset.set('file', full_path.replace('\\', '/'))
                
                # FIX: Unnamed materials from Menagerie will no longer overwrite each other!
                asset_id = asset.get('name', asset.get('file', f"unnamed_{asset.tag}_{id(asset)}"))
                if asset_id not in self.inserted_assets:
                    main_asset.append(asset)
                    self.inserted_assets.add(asset_id)

        # 3. Merge the Physics Brain (Defaults, Motors, Sensors, Collisions)
        physics_tags = ['default', 'actuator', 'sensor', 'tendon', 'contact', 'equality']
        for tag_name in physics_tags:
            agent_tag = agent_root.find(tag_name)
            if agent_tag is not None:
                main_tag = main_mujoco.find(tag_name)
                if main_tag is None:
                    main_tag = ET.SubElement(main_mujoco, tag_name)

                for child in list(agent_tag):
                    # Contacts use body1/body2 instead of names
                    if child.tag == 'exclude':
                        sig = f"exclude_{child.get('body1')}_{child.get('body2')}"
                    else:
                        sig = f"{tag_name}_{child.tag}_{child.get('name', '')}_{child.get('class', '')}"
                        
                    # Add to XML if it's uniquely named OR if it's completely unnamed
                    if sig not in self.inserted_physics_elements or "unnamed" in sig or not sig.replace('_', ''):
                        main_tag.append(child)
                        self.inserted_physics_elements.add(sig)

        # 4. The Master Wrapper
        agent_worldbody = agent_root.find('worldbody')
        if agent_worldbody is not None:
            wrapper_body = ET.Element('body', {'name': f"{name}__root", 'pos': pos_str, 'quat': quat_str})
            
            if is_mobile:
                ET.SubElement(wrapper_body, 'freejoint', {'name': f"{name}__freejoint"})

            for child in list(agent_worldbody):
                wrapper_body.append(child)
                
            main_worldbody.append(wrapper_body)
                
        print(f"[WorldBuilder] Stitched Native MJCF Agent '{name}' into memory. Mobile: {is_mobile}")