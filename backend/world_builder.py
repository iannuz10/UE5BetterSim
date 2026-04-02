import os
import xml.etree.ElementTree as ET
import logging

logger = logging.getLogger("WorldBuilder")

class WorldBuilder:
    def __init__(self, json_data):
        self.objects = json_data.get('objects', [])
        if self.objects is None:
            logger.critical("FATAL: JSON payload is missing the 'objects' array completely.")
            raise ValueError("Invalid JSON: Missing 'objects' array.")
        
        self.dynamic_props = []
        self.inserted_assets = set()
        self.inserted_physics_elements = set() 

        logger.info(f"Initialized WorldBuilder. Validating {len(self.objects)} objects: {[obj.get('name', 'Unnamed') for obj in self.objects]}")

    def build(self):
        main_mujoco = ET.Element('mujoco', {'model': 'RoboSimUE_World'})
        ET.SubElement(main_mujoco, 'compiler', {'angle': 'radian'})
        ET.SubElement(main_mujoco, 'option', {'gravity': '0 0 -9.81'})
        
        main_asset = ET.SubElement(main_mujoco, 'asset')
        main_worldbody = ET.SubElement(main_mujoco, 'worldbody')
        
        # Default lighting and ground grid
        ET.SubElement(main_worldbody, 'light', {'diffuse': '.5 .5 .5', 'pos': '0 0 10', 'dir': '0 0 -1'})
        ET.SubElement(main_asset, 'texture', {'type': 'skybox', 'builtin': 'gradient', 'rgb1': '0.3 0.5 0.7', 'rgb2': '0 0 0', 'width': '512', 'height': '512'})
        ET.SubElement(main_asset, 'texture', {'name': 'grid', 'type': '2d', 'builtin': 'checker', 'rgb1': '0.2 0.3 0.4', 'rgb2': '0.1 0.2 0.3', 'width': '512', 'height': '512'})
        ET.SubElement(main_asset, 'material', {'name': 'grid_mat', 'texture': 'grid', 'texrepeat': '1 1', 'texuniform': 'true', 'reflectance': '0.2'})

        for obj in self.objects:
            # --- STRICT VALIDATION (Fail-Fast) ---
            # We refuse to guess. If UE5 sends bad data, we crash and log it.
            if 'name' not in obj:
                logger.critical(f"FATAL: Received object with no name! Raw data: {obj}")
                raise ValueError("Missing 'name' in object payload.")
            
            name = obj['name']
            if 'mesh' not in obj:
                logger.critical(f"FATAL: Object '{name}' is missing 'mesh' type.")
                raise ValueError(f"Missing 'mesh' type for '{name}'.")
            
            mesh_type = obj['mesh'].lower()

            if not name:
                name = f"Unknown_Obj_{id(obj)}"
                logger.warning(f"Object missing 'name' in JSON. Assigned unique fallback: {name}")

            if 'position' not in obj or 'quat' not in obj:
                logger.critical(f"FATAL: Object '{name}' is missing spatial data (position or quat).")
                raise ValueError(f"Missing spatial data for '{name}'.")
            
            p = obj.get('position', [0,0,0])
            q = obj.get('quat', [1,0,0,0])
            pos_str = f"{p[0]} {p[1]} {p[2]}"
            quat_str = f"{q[0]} {q[1]} {q[2]} {q[3]}"

            # --- AGENTS (Robots) ---
            if mesh_type == 'agent' and obj.get('file_path'):
                logger.debug(f"Processing Agent: {name}")
                self._process_agent(obj, main_mujoco, main_asset, main_worldbody, pos_str, quat_str)
                continue
            
            # --- STATIC ENVIRONMENT ---
            if mesh_type in ['plane', 'floor']:
                if 'size' not in obj:
                    logger.critical(f"FATAL: Plane '{name}' is missing 'size'.")
                    raise ValueError(f"Missing 'size' for plane '{name}'.")
                size = obj['size']
                ET.SubElement(main_worldbody, 'geom', {
                    'name': name, 'type': 'plane', 'pos': pos_str, 'quat': quat_str, 
                    'size': f"{size[0]} {size[1]} 0.1", 'material': 'grid_mat'
                })
                continue
                
            if 'is_static' not in obj:
                logger.critical(f"FATAL: Object '{name}' is missing 'is_static' flag.")
                raise ValueError(f"Missing 'is_static' flag for '{name}'.")
            
            is_static = obj['is_static']
            
            # --- PROPS ---
            if not is_static:
                self.dynamic_props.append(name)
                
            prop_body = ET.SubElement(main_worldbody, 'body', {'name': name, 'pos': pos_str, 'quat': quat_str})
            
            # If it's dynamic, it needs a freejoint so gravity affects it.
            if not is_static:
                ET.SubElement(prop_body, 'freejoint', {'name': f"{name}_joint"})
            
            if mesh_type == 'cube':
                if 'size' not in obj:
                    logger.critical(f"FATAL: Cube '{name}' is missing 'size'.")
                    raise ValueError(f"Missing 'size' for cube '{name}'.")
                size = obj['size']
                ET.SubElement(prop_body, 'geom', {'type': 'box', 'size': f"{size[0]} {size[1]} {size[2]}", 'rgba': '0.8 0.2 0.2 1', 'mass': '50.0'})
                logger.debug(f"Spawned Cube: {name} | Size: {size} | Static: {is_static} | Pos: {pos_str} | Quat: {quat_str}")
            
            elif mesh_type == 'sphere':
                if 'size' not in obj:
                    logger.critical(f"FATAL: Sphere '{name}' is missing 'size'.")
                    raise ValueError(f"Missing 'size' for sphere '{name}'.")
                radius = obj['size'][0]
                ET.SubElement(prop_body, 'geom', {'type': 'sphere', 'size': str(radius), 'rgba': '0.2 0.2 0.8 1', 'mass': '1.0'})
                logger.debug(f"Spawned Sphere: {name} | Radius: {radius} | Static: {is_static} | Pos: {pos_str} | Quat: {quat_str}")
            
            elif mesh_type == 'custom':
                if 'file_path' not in obj:
                    logger.critical(f"FATAL: Custom mesh '{name}' is missing 'file_path'.")
                    raise ValueError(f"Missing 'file_path' for custom mesh '{name}'.")
                
                abs_mesh = os.path.abspath(obj['file_path'])
                
                # Check if file actually exists before telling MuJoCo to compile it
                if not os.path.exists(abs_mesh):
                    logger.critical(f"FATAL: Mesh file NOT FOUND on disk: '{abs_mesh}'")
                    raise FileNotFoundError(f"Missing mesh file for '{name}': {abs_mesh}")
                
                if 'scale' not in obj:
                    logger.critical(f"FATAL: Custom mesh '{name}' is missing 'scale' array.")
                    raise ValueError(f"Missing 'scale' for custom mesh '{name}'.")
            
                mesh_name = f"{name}_mesh"
                ue5_scale = obj['scale']

                # CM to Meters conversion for raw .obj vertices.
                # Assumes the .obj was cleanly exported as Z-Up / Y-Forward from Blender!
                mj_scale = f"{ue5_scale[0] * 0.01} {ue5_scale[1] * 0.01} {ue5_scale[2] * 0.01}"

                # Prevent duplicate asset loading in MuJoCo's global namespace. If two objects reference the same mesh file, we only insert it once.
                if mesh_name not in self.inserted_assets:
                    ET.SubElement(main_asset, 'mesh', {'name': mesh_name, 'file': abs_mesh, 'scale': mj_scale})
                    self.inserted_assets.add(mesh_name)
                
                # No fake rotations! Body inherits the pure UE5 C++ quaternion 
                ET.SubElement(prop_body, 'geom', {
                    'type': 'mesh', 
                    'mesh': mesh_name, 
                    'rgba': '0.7 0.7 0.7 1', 
                    'mass': '1.0'
                })
                logger.debug(f"Spawned Custom Mesh: {name} | Scale: {mj_scale} | Path: {abs_mesh} | Static: {is_static} | Pos: {pos_str} | Quat: {quat_str}")
            else:
                logger.critical(f"FATAL: Unrecognized mesh type '{mesh_type}' for object '{name}'.")
                raise ValueError(f"Unrecognized mesh type: {mesh_type}")

        xml_string = ET.tostring(main_mujoco, encoding='unicode')
        logger.info("Successfully built MuJoCo XML tree.")
        return xml_string, self.dynamic_props

    def _process_agent(self, obj, main_mujoco, main_asset, main_worldbody, pos_str, quat_str):
        name = obj['name']

        if 'file_path' not in obj:
            logger.critical(f"FATAL: Agent '{name}' missing 'file_path'.")
            raise ValueError(f"Missing 'file_path' for agent '{name}'.")
        
        if 'is_mobile' not in obj:
            logger.critical(f"FATAL: Agent '{name}' missing 'is_mobile' boolean.")
            raise ValueError(f"Missing 'is_mobile' flag for agent '{name}'.")

        mjcf_path = obj['file_path']
        is_mobile = obj['is_mobile']
        abs_mjcf_path = os.path.abspath(mjcf_path)
        # Define prefix here so it's available for asset name resolution below.
        prefix = f"{name}_"

        if not os.path.exists(abs_mjcf_path):
            logger.critical(f"FATAL: Agent MJCF file NOT FOUND on disk: '{abs_mjcf_path}'")
            raise FileNotFoundError(f"Missing MJCF for '{name}': {abs_mjcf_path}")

        mjcf_dir = os.path.dirname(abs_mjcf_path)

        try:
            agent_tree = ET.parse(abs_mjcf_path)
        except Exception as e:
            logger.critical(f"FATAL: XML Parsing failed for '{abs_mjcf_path}'. Error: {e}")
            raise ValueError(f"Failed to parse MJCF for '{name}': {e}")
            
        agent_root = agent_tree.getroot()

        # --- 1. MERGE COMPILER / MENAGERIE FIXES ---
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
                if key != 'gravity': 
                    main_option.set(key, val)

        # --- 2. ASSET RESOLUTION (Shared GPU Memory) ---
        agent_asset = agent_root.find('asset')
        if agent_asset is not None:
            for asset in list(agent_asset):
                if asset.tag in ['mesh', 'texture']:
                    mesh_file = asset.get('file')
                    if mesh_file and not os.path.isabs(mesh_file):
                        target_dir = texturedir if asset.tag == 'texture' else meshdir
                        full_path = os.path.normpath(os.path.join(mjcf_dir, target_dir, mesh_file))
                        asset.set('file', full_path.replace('\\', '/'))
                
                # For meshes, always assign an explicit prefixed name so MuJoCo's global asset
                # namespace is fully isolated per robot instance. Without this, two different robot
                # types whose files share the same stem (e.g. fr3/link1.obj and xarm7/link1.stl)
                # would produce "repeated name 'link1' in mesh" even though they have different file paths.
                if asset.tag == 'mesh':
                    explicit = asset.get('name')
                    if explicit:
                        stem = explicit
                    else:
                        f_path = asset.get('file', '')
                        stem = os.path.splitext(os.path.basename(f_path))[0] if f_path else f"unnamed_mesh_{id(asset)}"
                    asset.set('name', prefix + stem)
                    asset_id = prefix + stem
                else:
                    asset_id = asset.get('name', asset.get('file', f"unnamed_{asset.tag}_{id(asset)}"))
                if asset_id not in self.inserted_assets:
                    # Strip any 'class' reference — the class it pointed to will be renamed/prefixed
                    # later, so keeping it would create a dangling reference for shared assets.
                    if 'class' in asset.attrib:
                        logger.debug(f"[ASSET] Stripping 'class' ref from shared asset '{asset_id}'.")
                        del asset.attrib['class']
                    file_hint = asset.get('file', '<no file>')
                    main_asset.append(asset)
                    self.inserted_assets.add(asset_id)
                else:
                    file_hint = asset.get('file', '<no file>')

        # --- 3. THE NAMESPACE PREFIXER ---
        # To spawn multiple identical robots, every body, joint, motor, sensor, and mesh must have a unique name.
        # prefix was already defined above (before asset resolution).

        # Attributes that reference other named physics elements.
        # 'mesh' is included because mesh names are now prefixed per-instance in the asset loop above.
        # 'material' is excluded — materials are shared across all robots and not prefixed.
        ref_attributes = [
            'joint', 'joint1', 'joint2',
            'body', 'body1', 'body2',
            'geom', 'geom1', 'geom2',
            'site', 'tendon',
            'class', 'childclass',
            'mesh',
            'target'  # light/camera target= references a body by name
        ]

        def apply_prefix(elem):
            # 1. Rename the element itself
            if 'name' in elem.attrib:
                elem.set('name', prefix + elem.attrib['name'])
            
            # 2. Rename any attributes that point to other elements
            for attr in ref_attributes:
                if attr in elem.attrib:
                    elem.set(attr, prefix + elem.attrib[attr])
            
            # 3. Recurse down the tree
            for child in list(elem):
                apply_prefix(child)

        def prefix_default_tree(elem):
            # Prefix the 'class' definition name if present.
            # The root <default> has no 'class' attribute, so it is safely skipped.
            if 'class' in elem.attrib:
                elem.set('class', prefix + elem.attrib['class'])
            for child in list(elem):
                prefix_default_tree(child)

        # --- 4. PHYSICS BRAIN MERGE (Motors, Sensors, Contacts) ---
        # FIX: We MUST NOT deduplicate these! Every robot instance needs its own motors and sensors!
        physics_tags = ['actuator', 'sensor', 'tendon', 'contact', 'equality']
        for tag_name in physics_tags:
            agent_tag = agent_root.find(tag_name)
            if agent_tag is not None:
                main_tag = main_mujoco.find(tag_name)
                if main_tag is None:
                    main_tag = ET.SubElement(main_mujoco, tag_name)

                for child in list(agent_tag):
                    apply_prefix(child) # Rename the motor, fix its joint target
                    main_tag.append(child) # Add it straight to the world

        # Prefix all <default class="..."> definition names so each robot instance has a fully
        # isolated class namespace. Then always append the complete subtree — no deduplication
        # needed since all names are now unique (e.g. Robot1_iiwa, Robot2_iiwa).
        agent_default = agent_root.find('default')
        if agent_default is not None:
            prefix_default_tree(agent_default)
            main_default = main_mujoco.find('default')
            if main_default is None:
                main_default = ET.SubElement(main_mujoco, 'default')
            for child in list(agent_default):
                main_default.append(child)
            logger.debug(f"Appended prefixed <default> subtree for agent '{name}'.")

        # --- 5. THE WRAPPER BODY ---
        agent_worldbody = agent_root.find('worldbody')
        if agent_worldbody is not None:
            # Detect whether the MJCF's root body already owns a <freejoint>.
            # MuJoCo rule: a <freejoint> can ONLY appear on a top-level body (direct child of
            # <worldbody>). Wrapping such a body would nest it one level too deep → error:
            # "free joint can only be used on top level".
            root_body_with_freejoint = None
            for wc in list(agent_worldbody):
                if wc.tag == 'body' and any(gc.tag == 'freejoint' for gc in list(wc)):
                    root_body_with_freejoint = wc
                    break

            if root_body_with_freejoint is not None:
                # --- PATH A: MJCF-owned root freejoint (e.g. locomotion humanoids/quadrupeds) ---
                # Add worldbody children directly to main_worldbody so the freejoint stays top-level.
                # Rename the root body from its MJCF name (e.g. "pelvis") to "_root" BEFORE
                # apply_prefix runs, so the final name becomes "{name}__root" — this keeps
                # StatePublisher's parent-chain traversal working without changes.
                # Also patch any target= references to the original root body name (e.g. a light
                # that tracks the CoM) to "_root" so apply_prefix resolves them correctly.
                original_root_name = root_body_with_freejoint.get('name', '')
                root_body_with_freejoint.set('name', '_root')
                root_body_with_freejoint.set('pos', pos_str)
                root_body_with_freejoint.set('quat', quat_str)

                if original_root_name:
                    def patch_root_refs(elem):
                        if elem.get('target') == original_root_name:
                            elem.set('target', '_root')
                        for child_elem in list(elem):
                            patch_root_refs(child_elem)
                    patch_root_refs(agent_worldbody)

                skipped_lights = 0
                for child in list(agent_worldbody):
                    if child.tag == 'light':
                        skipped_lights += 1
                        continue  # Strip standalone visualization lights (see PATH B comment below)
                    apply_prefix(child)
                    main_worldbody.append(child)
                if skipped_lights:
                    logger.debug(f"Stripped {skipped_lights} MJCF visualization light(s) from agent '{name}'.")
                logger.debug(
                    f"Mobile agent '{name}' has own root freejoint. "
                    f"Added directly to worldbody as '{name}__root' (was '{original_root_name}')."
                )

            else:
                # --- PATH B: Standard case (fixed-base robot, or mobile with no own freejoint) ---
                # Robot MJCFs from Menagerie embed standalone visualization lights (trackcom, spotlight)
                # designed for single-robot viewing in MuJoCo's own renderer. In a multi-robot scene
                # these lights stack up, drastically altering scene lighting and adding render overhead.
                # Our build() method already provides scene lighting — strip all robot-internal lights.
                wrapper_body = ET.Element('body', {'name': f"{name}__root", 'pos': pos_str, 'quat': quat_str})
                if is_mobile:
                    ET.SubElement(wrapper_body, 'freejoint', {'name': f"{name}__freejoint"})
                    logger.debug(f"Added wrapper freejoint for mobile agent '{name}'.")
                skipped_lights = 0
                for child in list(agent_worldbody):
                    if child.tag == 'light':
                        skipped_lights += 1
                        continue
                    apply_prefix(child)
                    wrapper_body.append(child)
                if skipped_lights:
                    logger.debug(f"Stripped {skipped_lights} MJCF visualization light(s) from agent '{name}'.")
                main_worldbody.append(wrapper_body)
                
        logger.info(f"Stitched Native MJCF Agent '{name}' into memory. Mobile: {is_mobile} | Pos: {pos_str} | Quat: {quat_str}")