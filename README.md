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

## Prerequisites
Before cloning, ensure you have the following installed:

* Unreal Engine 5.7.3 (with C++ development tools / Raider or Xcode)
* Docker Desktop (with WSL 2 enabled on Windows)
* Git LFS (Large File Storage) - Important for pulling Unreal assets.
* MuJoCo Managerie - Clone the Managerie inside backend/assets/robots.

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
With the Unreal Engine project open and playing (Acting as the Zenoh Peer/Listener), execute the MuJoCo backend:
docker exec -it sim_brain python3 mujoco_parser.py

After the connection is established, press 'P' to start the simulation loop.
