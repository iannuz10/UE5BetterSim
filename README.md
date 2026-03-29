# RoboSimUE
## High-Fidelity Hybrid Robotics Simulation Infrastructure

RoboSimUE is a decoupled robotics simulation environment that bridges the photorealistic rendering of Unreal Engine 5 with the scientific physics and machine learning ecosystems of MuJoCo/ROS 2.

By utilizing Zenoh as a high-performance Data Distribution Service (DDS) alternative, this project achieves (preliminary) sub-3ms latency between the visual host (Windows/Mac) and the logical brain (Docker/Linux).

## Architecture
The system operates on a Host-Guest Topology:

* The Host (Unreal Engine 5): Acts as the "Body" and "Eyes". Handles high-fidelity rendering (Nanite/Lumen), sensor simulation, and kinematic visualization.
* The Guest (Docker Container): Acts as the "Brain". Runs a containerized Linux environment with Python, ROS 2, and physics engines (MuJoCo).
* The Bridge (Zenoh 1.7.2): A lightweight, zero-overhead C++ plugin integrated into Unreal Engine that communicates with the Python backend via TCP.

## Current Features
The project currently features a fully functional, bidirectional physics loop running at 60 FPS:
* Cross-Platform C++ Plugin: Custom ZenohBridge module supporting Windows (x64) and macOS (Apple Silicon).
* Low-Latency TCP Bridge: Direct Client-to-Listener connection bypassing standard mesh-routing overhead (averaging ~2.9ms RTT).
* Blueprint Integration: Custom Unreal Engine components exposing network events directly to the Event Graph.
* Dynamic World Generation: Unreal Engine automatically serializes the environment (static floors, dynamic props, custom meshes) into JSON. The Python backend dynamically builds a MuJoCo worldbody entirely in memory.
* Robot Support (MJCF): Seamlessly loads native .mjcf / .xml robotic models, fully supporting industry-standard repositories like mujoco_menagerie.
* Smart Physics Stitching: The Python engine automatically injects gravity for mobile robots (Humanoids/Quadrupeds), and physically bolts fixed manipulators (Kuka) to their exact UE5 spawn coordinates.
* Asset & Physics Deduplication: Safe memory management allows spawning multiple instances of the same robot without duplicating heavy meshes, actuators, or contact exclusion rules.
* Kinematic State Extraction: MuJoCo runs the forward-dynamics physics and publishes specific joint arrays (hinge/slide states) and dynamic prop transforms back to Unreal Engine over isolated Zenoh topics.

## Envisioned Capabilities & Applications
The project is born with an agnostic and modular architecture in mind. UE5 just receives positional updates for dynamic objects and joint states updates from any kind of external physics engine. In fact the objective is to not depend solely on MuJoCo, but offer the possibility to swap the underlying physics engine by preference. That could be MuJoCo, Bullet, Jolt, PhysX. This would be achieved by the implementation of a Digital Twin from UE5 to the physics engine of choice. Based on the use case one would prefer one engine to another.

Regarding the Applications, many could be the possibilities:
* Synthetic data generation for World Models/VAs/VLAs: Starting with a randomly generated prompt (a words bank and a defined structure would be used), the procedural generation capabilities of UE5 will make possible the generation of random environments, starting points, actions and goals will be extracted by the initial prompt and associated to objects in the UE5 environment. In the background ROS2 and the external physics engine will plan and execute the necessary actions, while a camera mounted on the robot will record labeled, realistic image data.
* Graphically and physically realistic simulator for robotics. Robotic simulators are not able to achieve both realistic looks and realistic physics, not without a tradeoff of performances. By decoupling the physics and the rendering, one could decide to use two separate machines to dedicate the computational power to solely one of the two tasks, without the need of crazy setups (also on a single machine this would work of course, even in cloud hypothetically speaking).
* RL training would be also possible with the right amount of effort put in the management of multiple instances. This would probably not match the performance of softwares like Isaac Sim, but if the training would be made over rgb images, maybe the high quality images obtained by the realistic render from UE5 would be worth more.
* VR teleoperation is also in the radar. By creating a 1-to-1 copy of the destination environment it would be possible to generate a rendered world in both UE5 for visual fidelity and a virtual physical digital twin in the physics engine. By fusing the data gathered for example from sensors in the destination room, smooth teleoperated control would be possible with high visual and physical fidelituy while being completely immersed in the virtual copy of the environment, while also having real results in the teleoperated room.

## Prerequisites
Before cloning, ensure you have the following installed:

* Unreal Engine 5.7.3 (with C++ development tools / Raider or Xcode)
* Docker Desktop (with WSL 2 enabled on Windows)
* Git LFS (Large File Storage) - Important for pulling Unreal assets.
* MuJoCo Managerie - Clone the Managerie inside backend/assets/robots.

## Mesh Creation for UE5
An automated pipeline is not ready yet. Right now the main source for robot models is the MuJoCo Managerie (the .xml files).

To be able to have a Blueprint with all the assets in UE5 that follow the hierarchy of the original mjcf files do as follows:

1. Choose the robot from the MuJoCo Managerie folder and run the command: python3 visual_urdf_generator.py ./assets/robots/[robot folder]/[robot name].xml (only the robot, not the scene).
2. A .urdf will be created. The urdf will contain only the information of the frames and the visuals. Open Blender and the LinkForge plugin panel.
3. Import the .urdf through LinkForge. Only the robot should be present, no other meshes, cameras, nothing else.
4. Select all the robotics parts and then from the menu bar on top go to File > Export > Export as gltf (could be blg/gltf2 depending on the version).
5. In the export verify the path and the name of the .glb file. Also make sure that "Animation" is unchecked, that the "+Y" is checked and to check "Export selected". Then export.
6. The Blender side is done. Go to UE5 and click in the menu bar on top File > File > Import into Level and select the .glb file exported from Blender. Then select the destination directory of the import (RoboSim/Robots/). In the menu that pops up select "Scene" and the Pipeline should be the second one. Dont change anything else.
7. This will import the assets separately in the World at it's origin. If you dont see it it will be in the World (0,0,0) coordinates. 
8. In the outliner, select all the assets under the Scene asset just imported > press the blueprint icon in the top bar > Convert Selection to Blueprint class > select Harvest Components, inherit from “Actor” and rename the BP.
9. A new window will be created with all the assets correctly inherited.
10. Next step would be setting up the actuated joints, which right now may be tedious cause it's manual. We'll try to automate this part.

## Installation & Setup

### 1. Clone the Repository
Because this project contains Unreal assets, you must have Git LFS installed before cloning:

git lfs install
git clone https://github.com/iannuz10/UE5BetterSim.git
cd UE5BetterSim/RoboSimUE

### 2. Build the Unreal Project (Windows & Mac)
The project includes a custom C++ plugin. You need to generate the IDE files and compile the binaries for your specific OS.

1. Right-click on RoboSimUE.uproject.
2. Select Generate Visual Studio project files (Windows) or Generate Xcode project (Mac).
3. Open the generated solution (.sln or .xcworkspace) and Build the project.
4. Launch RoboSimUE.uproject.

### 3. Setup the Docker "Brain"
The Python logic lives inside a Docker container to ensure environment consistency across the team.

# Build the Docker image
docker build -t robosim_brain ./backend

# Run the project
With the Unreal Engine project open and playing (Acting as the Zenoh Peer/Listener), execute the MuJoCo backend from the container:
python3 mujoco_parser.py

After the connection is established, press 'P' to start the simulation loop.
