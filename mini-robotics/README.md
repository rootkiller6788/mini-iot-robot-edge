# mini-robotics — 机器人技术 (C 语言实现)

A comprehensive C99 robotics library implementing core robot control algorithms:
forward/inverse kinematics, path planning, SLAM, ROS communication model,
and mobile robot control.

## Features

| Module | Description |
|--------|-------------|
| **kinematics** | DH parameters, forward kinematics (4×4 chain), numerical IK (Jacobian inverse), analytical IK (3-DOF planar), workspace analysis |
| **path_planning** | A* and Dijkstra grid search, RRT and RRT* sampling-based planners, collision checking, repulsive potential fields, trajectory spline generation |
| **slam_system** | EKF-SLAM (pose + landmarks), FastSLAM (particle filter), GraphSLAM (pose graph optimization), loop closure detection, occupancy grid mapping |
| **ros_core** | Topic publish/subscribe, service call/advertise, action client/server, message serialization, bag file I/O, TF2 transform tree, launch file parsing |
| **mobile_robot** | Differential drive kinematics/odometry, IMU orientation estimation, AMCL Monte Carlo localization, laser scan processing, PID control, navigation stack (go-to-goal + obstacle avoidance) |

## Directory Structure

```
mini-robotics/
├── include/              # Header files
│   ├── kinematics.h
│   ├── path_planning.h
│   ├── slam_system.h
│   ├── ros_core.h
│   └── mobile_robot.h
├── src/                  # Implementation files
│   ├── kinematics.c
│   ├── path_planning.c
│   ├── slam_system.c
│   ├── ros_core.c
│   └── mobile_robot.c
├── examples/             # Example programs
│   ├── ex1_forward_kinematics.c
│   ├── ex2_path_planning.c
│   └── ex3_slam_demo.c
├── demos/                # Demo READMEs
├── docs/                 # Documentation
│   ├── API_REFERENCE.md
│   └── THEORY.md
├── benches/              # Benchmarks
├── tests/                # Test stubs
├── Makefile
└── README.md
```

## Building

### Requirements
- C99-compatible compiler (GCC, Clang, MSVC)
- `math.h` (libm)

### Compile

```bash
# Build library + examples
make all

# Build only the static library
make lib

# Build examples
make examples

# Clean
make clean
```

### Manual Compilation

```bash
# Compile library object files
gcc -std=c99 -O2 -c src/kinematics.c -o build/kinematics.o -Iinclude
gcc -std=c99 -O2 -c src/path_planning.c -o build/path_planning.o -Iinclude
gcc -std=c99 -O2 -c src/slam_system.c -o build/slam_system.o -Iinclude
gcc -std=c99 -O2 -c src/ros_core.c -o build/ros_core.o -Iinclude
gcc -std=c99 -O2 -c src/mobile_robot.c -o build/mobile_robot.o -Iinclude

# Create static library
ar rcs build/libmini-robotics.a build/*.o

# Compile example
gcc -std=c99 -O2 examples/ex1_forward_kinematics.c -o build/ex1 \
    -Iinclude -Lbuild -lmini-robotics -lm
```

## Quick Start

### Forward Kinematics

```c
#include "kinematics.h"
#include <stdio.h>

int main(void) {
    mr_robot_desc robot;
    robot.num_joints = 3;

    robot.joints[0].dh = (mr_dh_param){0.0, -M_PI_2, 0.3, 0.0};
    robot.joints[1].dh = (mr_dh_param){0.5, 0.0, 0.0, 0.0};
    robot.joints[2].dh = (mr_dh_param){0.3, 0.0, 0.0, 0.0};

    double q[3] = {0.0, M_PI_4, -M_PI_4};
    mr_pose end_effector = mr_forward_kinematics(&robot, q, 3);

    printf("End effector: (%.3f, %.3f, %.3f)\n",
           end_effector.x, end_effector.y, end_effector.z);
    return 0;
}
```

### A* Path Planning

```c
#include "path_planning.h"
#include <stdio.h>

int main(void) {
    mr_grid_map map;
    map.width = 100;
    map.height = 100;
    map.resolution = 0.1;
    map.origin_x = -5.0;
    map.origin_y = -5.0;

    mr_path2d path;
    mr_point2d start = {0.0, 0.0};
    mr_point2d goal = {5.0, 5.0};

    if (mr_a_star(&map, &start, &goal, &path))
        printf("Path found with %d waypoints\n", path.num_waypoints);

    return 0;
}
```

### Differential Drive Odometry

```c
#include "mobile_robot.h"
#include <stdio.h>

int main(void) {
    mr_diff_drive_model model;
    mr_diff_drive_setup(&model, 0.05, 0.25, 1024.0);

    mr_robot_state_2d state = {0};
    mr_odometry_update(&model, 100.0, 120.0, &state);

    printf("Robot pose: (%.3f, %.3f, %.3f rad)\n",
           state.x, state.y, state.theta);
    return 0;
}
```

## API Overview

### Kinematics (`kinematics.h`)
- `mr_dh_transform()` — compute 4×4 transformation from DH parameters
- `mr_forward_kinematics()` — chain multiplication for end-effector pose
- `mr_inverse_kinematics_numerical()` — Jacobian pseudo-inverse IK solver
- `mr_build_jacobian()` — geometric Jacobian computation
- `mr_workspace_check()` — point-in-workspace verification

### Path Planning (`path_planning.h`)
- `mr_a_star()` — grid-based shortest path with Euclidean heuristic
- `mr_dijkstra()` — uniform cost search
- `mr_rrt_plan()` — single-direction RRT
- `mr_rrt_star_plan()` — optimal RRT* with rewiring
- `mr_repulsive_potential()` — obstacle repulsion force field

### SLAM (`slam_system.h`)
- `mr_ekf_slam_predict()` — motion model update
- `mr_ekf_slam_update()` — landmark observation correction
- `mr_fast_slam_update()` — particle weight update
- `mr_graph_slam_optimize()` — iterative pose graph relaxation
- `mr_occ_grid_update_from_scan()` — laser scan → occupancy grid

### ROS Core (`ros_core.h`)
- `mr_node_subscribe()` / `mr_node_publish()` — topic communication
- `mr_message_serialize()` / `mr_message_deserialize()` — binary format
- `mr_bag_writer_open()` / `mr_bag_writer_write()` — record rosbag
- `mr_tf_tree_add_transform()` / `mr_tf_tree_lookup()` — coordinate frames
- `mr_launch_file_parse()` — XML launch file parsing

### Mobile Robot (`mobile_robot.h`)
- `mr_odometry_update()` — encoder → pose
- `mr_imu_compute_orientation()` — gyro/accel/mag → quaternion
- `mr_amcl_predict()` / `mr_amcl_update_from_laser()` — Monte Carlo localization
- `mr_goto_goal()` — proportional goal-seeking controller
- `mr_navigation_stack()` — combined global+local planner

## License

MIT License. See LICENSE file for details.
