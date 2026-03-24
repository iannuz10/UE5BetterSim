#####WIP#####
#####WIP#####
#####WIP#####
#####WIP#####

import os
import mujoco
import xml.etree.ElementTree as ET

def convert_urdf_to_mjcf(urdf_path):
    """
    Takes a path to a .urdf file, safely compiles it, and returns 
    the path to a generated .mjcf.xml file.
    """
    abs_urdf_path = os.path.abspath(urdf_path)
    base_dir = os.path.dirname(abs_urdf_path)
    file_name = os.path.basename(abs_urdf_path)
    
    # The final output file we want to generate
    out_mjcf_path = abs_urdf_path.replace('.urdf', '.mjcf.xml')
    
    # 1. Skip if we already successfully converted this file and the URDF hasn't changed.
    if os.path.exists(out_mjcf_path):
        if os.path.getmtime(out_mjcf_path) >= os.path.getmtime(abs_urdf_path):
            return out_mjcf_path

    print(f"[Converter] Translating '{file_name}' to MuJoCo format...")

    # 2. Parse the original URDF cleanly using an XML tree (No messy Regex!)
    tree = ET.parse(abs_urdf_path)
    root = tree.getroot()

    # 3. Safely inject MuJoCo's required compiler settings
    # We check if a <mujoco> tag already exists. If not, we create one.
    mujoco_tag = root.find('mujoco')
    if mujoco_tag is None:
        mujoco_tag = ET.Element('mujoco')
        # Insert it right at the top of the URDF, just inside <robot>
        root.insert(0, mujoco_tag) 

    # We add/overwrite the <compiler> tag inside <mujoco>
    compiler_tag = mujoco_tag.find('compiler')
    if compiler_tag is None:
        compiler_tag = ET.SubElement(mujoco_tag, 'compiler')

    # These three settings are mandatory for URDFs to work in MuJoCo:
    # - strippath="false": Tells MuJoCo NOT to delete "meshes/" from your file paths.
    # - balanceinertia="true": Fixes broken physics math imported from CAD software.
    # - discardvisual="false": Ensures visual-only meshes aren't deleted.
    compiler_tag.set('strippath', 'false')
    compiler_tag.set('balanceinertia', 'true')
    compiler_tag.set('discardvisual', 'false')

    # 4. Save this modified XML to a temporary file. 
    # We do this so we NEVER alter your original source URDF.
    temp_urdf_path = os.path.join(base_dir, f"_temp_{file_name}")
    tree.write(temp_urdf_path)

    # 5. Compile using MuJoCo's native engine
    original_cwd = os.getcwd()
    try:
        # We MUST change directory to the URDF's folder so MuJoCo can find the local .obj/.stl meshes
        os.chdir(base_dir)
        
        # Load the temp file into MuJoCo
        model = mujoco.MjModel.from_xml_path(temp_urdf_path)
        
        # Save the optimized result as our final .mjcf.xml file
        mujoco.mj_saveLastXML(out_mjcf_path, model)
        print(f"[Converter] Success! Generated '{os.path.basename(out_mjcf_path)}'")
        
    except Exception as e:
        print(f"[Converter] FATAL ERROR compiling URDF: {e}")
        raise e # Crash loudly if the URDF is fundamentally broken
        
    finally:
        # 6. Cleanup: Always return to the original directory and delete the temp file
        os.chdir(original_cwd)
        if os.path.exists(temp_urdf_path):
            os.remove(temp_urdf_path)

    return out_mjcf_path