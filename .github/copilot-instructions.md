# GitHub Copilot / AI Agent Instructions for RoboSimUE

Purpose: give an AI coding assistant immediate, actionable context to work productively in this repo.

1) Big-picture architecture
- Host (Unreal Engine 5): visual renderer & sensors. See [README.md](README.md#Architecture).
- Guest (Docker/Python): physics, control, and ML stacks live under `backend/` (MuJoCo, ROS tooling).
- Bridge (ZenohBridge plugin): C++ UE plugin in `RoboSimUE/Plugins/ZenohBridge` that forwards JSON payloads over TCP to the Python backend.

2) Where to start (concrete files)
- `backend/urdf_to_mjcf.py` — URDF → MJCF conversion (safe temp-file flow, mujoco API usage, XML edits). Example pattern: conversion writes a `_temp.urdf`, compiles with `mujoco.MjModel.from_xml_path(...)`, and writes `<name>_instance.xml`.
- `backend/windows_listener.py`, `backend/test_bridge.py`, `backend/latency_test.py` — network bridge test/diagnostics.
- `backend/Dockerfile` and `backend/docker-compose.yml` — how the Guest environment is built and run.
- `RoboSimUE/Plugins/ZenohBridge` — plugin source; modify & rebuild UE project to update bridge behavior.

3) Build & run workflows (essential commands)
- Build Docker image for the brain (from repo root):
```bash
docker build -t robosim_brain ./backend
```
- Generate and build UE project (Windows): right-click `RoboSimUE.uproject` → Generate Visual Studio project files → open `.sln` and Build. Logs: `RoboSimUE/Saved/Logs`.

4) Project-specific patterns & conventions
- Do not edit original URDFs in place. Converters write `*_temp.urdf` and output `*_instance.xml` in the same folder.
- XML edits in `urdf_to_mjcf.py` currently use regex to strip/insert a `<mujoco>` block — this is brittle. Prefer using `xml.etree.ElementTree` or `lxml` to remove existing `<mujoco>` nodes and insert a canonical one.
- Naming: `name__root` bodies, `name_freejoint` for mobile agents — keep those conventions when programmatically generating bodies.
- Mesh paths assume the working directory is the mesh base; conversion temporarily `chdir`s to the URDF folder before compilation.

5) Integration & communication notes
- Data is JSON-serialized in the bridge; the UE plugin sends/receives lightweight JSON messages over TCP/Zenoh.
- The Guest (Docker) expects meshes and URDF/MJCF files accessible relative to the conversion working dir — watch for path mismatches when testing locally vs in-container.

6) Debugging tips
- UE-side: check `RoboSimUE/Saved/Logs/` and the Unreal Editor Output window after launching the map.
- Backend: run `python backend/test_bridge.py` and `python backend/latency_test.py` for connectivity and latency checks.
- Conversion failures: inspect temporary `_temp.urdf` and the mujoco-generated `.mjcf.xml` next to it; `urdf_to_mjcf.py` prints conversion status.

7) Safety for automated edits
- Avoid touching large asset directories under `Content/` or `DerivedDataCache/` — these are binary and managed with Git LFS.
- When changing C++ plugin behavior, regenerate the IDE project and rebuild UE binaries rather than trying to hot-swap code.

8) Examples for common tasks
- Add a new robot model: place URDF + meshes under `backend/assets/robots/<name>/`, run the conversion flow (`urdf_to_mjcf.process_agent`) and check `<name>_instance.xml`.
- Update bridge message structure: change serialization in `RoboSimUE/Plugins/ZenohBridge` and mirror parsing logic in `backend/windows_listener.py`.

9) When in doubt
- Prefer small, reversible changes; run `backend/test_bridge.py` after edits.
- If modifying XML, use ElementTree and preserve original URDF files (don't overwrite them).

Feedback: If any section is unclear or you want more examples (e.g., exact message schemas, conversion unit tests), tell me which area to expand.
