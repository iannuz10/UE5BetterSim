# RoboSimUE
## High-Fidelity Hybrid Robotics Simulation Infrastructure

RoboSimUE is a decoupled robotics simulation environment that bridges the photorealistic rendering of Unreal Engine 5 with the scientific physics and machine learning ecosystems of MuJoCo and ROS 2.

By utilizing Zenoh as a high-performance Data Distribution Service (DDS) alternative, this project achieves **sub-3ms latency** (averaging ~2ms RTT) between the visual **Host** (Windows/Mac) and the logical **Guest** (Docker/Linux).

> **Note on Latency:** Current RTT measurements are taken while running the Guest inside a Docker container. These values are largely influenced by the OS scheduler and Docker overhead; they represent the current operational performance rather than the theoretical limit of the Zenoh backbone.

## Architecture
The system operates on a Host-Guest Topology:

* **The Host (Unreal Engine 5.7+):** Acts as the "Body" and "Eyes". It handles high-fidelity rendering (Nanite/Lumen), sensor simulation, and kinematic visualization.
* **The Guest (Dockerized Linux):** Acts as the "Brain". It runs a containerized environment with Python, MuJoCo, and ROS 2. The backend is modularized into specialized managers (`Bridge`, `Engine`, `Telemetry`) for high extensibility.
* **The Bridge (Zenoh):** A lightweight, zero-overhead PUB/SUB networking layer that connects the Host and Guest via TCP, optimized for real-time digital twinning.

## Current Features & Objectives
* **Modular Backend Architecture:** The system is explicitly designed to be physics-engine agnostic. While **MuJoCo is currently the primary engine**, the modular bridge and engine managers are built to eventually support swapping for Bullet, Jolt, or PhysX as a core project objective.
* **Bridge Layer Flexibility:** A long-term goal is to achieve bridge-layer agnosticism. While Zenoh is the current backbone, the architecture is being structured to eventually allow swapping the networking layer for other DDS alternatives (e.g., FastDDS or CycloneDDS) based on specific infrastructure needs.
* **Stable 60Hz Physics Loop:** A fully functional, bidirectional physics-to-visual loop using isolated Zenoh topics for state synchronization.
* **Dynamic World Generation:** UE5 serializes the environment (static floors, dynamic props, custom meshes) into JSON. The Guest dynamically builds the physical worldbody entirely in memory.
* **Robot Support (MJCF):** Seamlessly loads native .mjcf / .xml robotic models, fully supporting industry-standard repositories like `mujoco_menagerie`.

## Envisioned Capabilities & Applications
RoboSimUE is designed with a modular "Digital Twin" philosophy:

* **Synthetic Data Generation for World Models:** By leveraging UE5’s procedural generation, the project enables the creation of random environments and goals for training World Models, VAs, and VLAs. A virtual camera on the robot records labeled, high-fidelity image data for vision-based learning.
* **Decoupled High-Performance Simulation:** Traditional simulators often trade visual fidelity for physics performance. By decoupling the two, computational power can be dedicated to each task independently, even across separate machines or cloud instances.
* **Reinforcement Learning (RL):** While not intended to replace high-throughput engines like Isaac Sim, RoboSimUE offers a path for training over realistic RGB images, where the high-fidelity rendering of UE5 provides superior domain-randomization value.
* **VR Teleoperation & HRI:** Create a 1-to-1 virtual digital twin of a real-world environment. By fusing sensor data into the UE5 visual layer, users can teleoperate robots with high physical fidelity while being completely immersed in a realistic virtual copy.

## Mesh Creation Pipeline (MJCF → URDF → UE5)
Importing robotic assets from MuJoCo into Unreal Engine requires a specific pipeline to ensure hierarchy and visual transforms are preserved:

1. **XML to URDF:** Choose your robot from `backend/assets/robots/` and run:
   `python3 visual_urdf_generator.py ./assets/robots/[robot_folder]/[robot].xml`
   *This generates a visual-only .urdf that corrects MuJoCo's internal mesh offsets.*
2. **Blender Intermediate:**
   - Open Blender and use the **LinkForge** plugin to import the `.urdf`.
   - Select the robot parts and go to `File > Export > glTF 2.0 (.glb)`.
   - **Settings:** Uncheck "Animation", check "+Y Up", and check "Export Selected".
3. **UE5 Import:**
   - In Unreal Engine: `File > Import into Level` and select the `.glb`.
   - Import into `RoboSim/Robots/`. In the popup, select **Scene** pipeline.
4. **Blueprint Conversion:**
   - Select all imported assets in the Outliner.
   - Click the **Blueprint** icon -> **Convert Selection to Blueprint Class**.
   - Select **Harvest Components** and inherit from **Actor**.

## Installation & Setup

### 1. Clone the Repository
```bash
git lfs install
git clone https://github.com/iannuz10/UE5BetterSim.git
```

### 2. Build the Unreal Project
1. Right-click `RoboSimUE.uproject` -> **Generate Visual Studio project files**.
2. Open the solution and **Build** the project.
3. Launch the project in the Unreal Editor.

### 3. Setup the Docker Guest
```bash
# Build the Docker image
docker build -t robosim_brain ./backend

# Run the physics loop inside the container
python3 sim_core.py
```
With the Unreal project playing, press **'P'** inside the UE5 window to trigger the `sim/world/init` payload and begin the simulation.
