import mujoco
import numpy as np
import os
import xml.etree.ElementTree as ET
import argparse

# ==========================================
# MINIMAL MATH HELPERS
# ==========================================
def q_mul(q1, q2):
    w1, x1, y1, z1 = q1
    w2, x2, y2, z2 = q2
    return [w1*w2 - x1*x2 - y1*y2 - z1*z2, w1*x2 + x1*w2 + y1*z2 - z1*y2,
            w1*y2 - x1*z2 + y1*w2 + z1*x2, w1*z2 + x1*y2 - y1*x2 + z1*w2]

def q_inv(q): return [q[0], -q[1], -q[2], -q[3]]

def apply_q(q, v):
    res = q_mul(q_mul(q, [0, v[0], v[1], v[2]]), q_inv(q))
    return np.array(res[1:4])

def quat_to_rpy(q):
    w, x, y, z = q
    r = np.arctan2(2*(w*x + y*z), 1 - 2*(x*x + y*y))
    p = np.arcsin(np.clip(2*(w*y - z*x), -1.0, 1.0))
    y = np.arctan2(2*(w*z + x*y), 1 - 2*(y*y + z*z))
    return r, p, y

# ==========================================
# PATH HUNTER
# ==========================================
def get_mesh_paths(xml_path):
    paths = {}
    def _parse(filepath, meshdir=""):
        try:
            root = ET.parse(filepath).getroot()
            comp = root.find('compiler')
            if comp is not None and comp.get('meshdir'): meshdir = comp.get('meshdir')
            for asset in root.iter('asset'):
                for mesh in asset.findall('mesh'):
                    if mesh.get('file'):
                        name = mesh.get('name', os.path.splitext(os.path.basename(mesh.get('file')))[0])
                        paths[name] = os.path.normpath(os.path.join(meshdir, mesh.get('file'))).replace('\\', '/')
            for inc in root.iter('include'):
                if inc.get('file'): _parse(os.path.join(os.path.dirname(filepath), inc.get('file')), meshdir)
        except: pass
    _parse(xml_path)
    return paths

# ==========================================
# GENERATOR
# ==========================================
def generate_urdf(mjcf_path):
    print(f"[RoboSim] Converting {mjcf_path} to Visual URDF...")
    model = mujoco.MjModel.from_xml_path(mjcf_path)
    paths = get_mesh_paths(mjcf_path)
    
    urdf = [f'<robot name="{os.path.basename(os.path.dirname(mjcf_path))}">']
    
    # Iterate bodies, starting at 1 to completely ignore the useless "world" body
    for i in range(1, model.nbody):
        bname = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_BODY, i) or f"body_{i}"
        urdf.append(f'  <link name="{bname}">')
        
        for j in range(model.ngeom):
            # THE UNIVERSAL FILTER: Must be attached to this body, must be a mesh, and MUST be Group < 3
            if model.geom_bodyid[j] == i and model.geom_type[j] == mujoco.mjtGeom.mjGEOM_MESH and model.geom_group[j] < 3:
                
                mesh_id = model.geom_dataid[j]
                mname = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_MESH, mesh_id) or ""
                filepath = paths.get(mname, f"{mname}.obj")

                # Undo MuJoCo's internal mesh shifting
                g_pos, g_quat = model.geom_pos[j], model.geom_quat[j]
                m_pos, m_quat = model.mesh_pos[mesh_id], model.mesh_quat[mesh_id]
                
                inv_m_pos = -apply_q(q_inv(m_quat), m_pos)
                urdf_pos = g_pos + apply_q(g_quat, inv_m_pos)
                urdf_quat = q_mul(g_quat, q_inv(m_quat))
                
                r, p, yaw = quat_to_rpy(urdf_quat)
                scale = model.mesh_scale[mesh_id]
                
                # Get exact Color
                rgba = model.mat_rgba[model.geom_matid[j]] if model.geom_matid[j] != -1 else model.geom_rgba[j]
                
                urdf.append('    <visual>')
                urdf.append(f'      <origin xyz="{urdf_pos[0]:.6f} {urdf_pos[1]:.6f} {urdf_pos[2]:.6f}" rpy="{r:.6f} {p:.6f} {yaw:.6f}"/>')
                urdf.append(f'      <geometry><mesh filename="{filepath}" scale="{scale[0]:.6f} {scale[1]:.6f} {scale[2]:.6f}"/></geometry>')
                urdf.append(f'      <material name="mat_{mname}"><color rgba="{rgba[0]:.3f} {rgba[1]:.3f} {rgba[2]:.3f} {rgba[3]:.3f}"/></material>')
                urdf.append('    </visual>')
                
        urdf.append(f'  </link>')
        
        # JOINT GENERATION
        pid = model.body_parentid[i]
        
        # If the parent is world (0), skip joint creation so this body becomes the clean root
        if pid == 0: 
            continue
            
        pname = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_BODY, pid) or f"body_{pid}"
        
        jname, jtype, axis = f"fixed_{pname}_to_{bname}", "fixed", [0,0,1]
        for k in range(model.njnt):
            if model.jnt_bodyid[k] == i:
                jname = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_JOINT, k) or f"jnt_{k}"
                jtype, axis = "continuous", model.jnt_axis[k]
                break
                
        r, p, yaw = quat_to_rpy(model.body_quat[i])
        b_pos = model.body_pos[i]
        
        urdf.append(f'  <joint name="{jname}" type="{jtype}">')
        urdf.append(f'    <parent link="{pname}"/>')
        urdf.append(f'    <child link="{bname}"/>')
        urdf.append(f'    <origin xyz="{b_pos[0]:.6f} {b_pos[1]:.6f} {b_pos[2]:.6f}" rpy="{r:.6f} {p:.6f} {yaw:.6f}"/>')
        if jtype != "fixed": urdf.append(f'    <axis xyz="{axis[0]:.6f} {axis[1]:.6f} {axis[2]:.6f}"/>')
        urdf.append(f'  </joint>')

    urdf.append('</robot>')
    
    out = mjcf_path.replace('.xml', '_visual_only.urdf')
    with open(out, 'w') as f: 
        f.write("\n".join(urdf))
        
    print(f"[RoboSim] SUCCESS! Clean URDF generated: {out}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('mjcf_file', type=str)
    generate_urdf(parser.parse_args().mjcf_file)