import yourdfpy

# yourdfpy safely parses the URDF XML and resolves the mesh paths
robot = yourdfpy.URDF.load('/app/Robots/nao_humanoid/nao.urdf')

# Access the underlying trimesh scene and export it
robot.scene.export('/app/Robots/nao_humanoid/nao_ready.glb')
print("Successfully exported to GLB!")