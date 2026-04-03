# Implementation Plan: `RoboSimAgent` C++ Base Class (Host & Bridge Update)

## Tasks

- [x] Task 1: Update JSON Contract (`rules/common/json-contract.md`) to include `root_transform` in `sim/agent/{agent_name}/state`. [d503246]
- [x] Task 2: Update Python Backend (`backend/mujoco_parser.py`) to extract and inject `root_transform` into agent state. [c7e7710]
- [x] Task 3: Create `ARoboSimAgent` C++ Class headers and cpp files with `JointsCache`, `RootFrameComponent`, and `bIsMobile`. [da96740]
- [x] Task 4: Implement `CacheJointComponents()` in `ARoboSimAgent`. [ee3ec16]
- [~] Task 5: Implement `ApplyUnifiedState()` in `ARoboSimAgent` handling both root transform and joints.
- [ ] Task 6: Add Blueprint Integration Hooks (reparent Blueprints, update Zenoh graph callbacks).
- [ ] Task 7: Review & Refine (validate performance and smooth updates).