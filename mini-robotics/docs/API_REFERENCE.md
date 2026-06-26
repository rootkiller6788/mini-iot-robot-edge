# mini-robotics — API Reference

## kinematics.h

### Types

| Type | Description |
|------|-------------|
| `mr_dh_param` | Denavit-Hartenberg parameter: `a`, `alpha`, `d`, `theta` |
| `mr_mat4` | 4×4 homogeneous transformation matrix, row-major |
| `mr_pose` | 6-DOF pose: `(x, y, z, roll, pitch, yaw)` |
| `mr_robot_state` | Robot state including pose, joint angles, gripper |
| `mr_twist` | 6-DOF velocity: `(vx, vy, vz, wx, wy, wz)` |
| `mr_jacobian` | 6×n Jacobian matrix mapping joint velocities to end-effector twist |
| `mr_joint_type` | Enum: `MR_JOINT_REVOLUTE` or `MR_JOINT_PRISMATIC` |
| `mr_joint` | Joint descriptor: type, DH params, limits, current value |
| `mr_robot_desc` | Full robot description: joints array, count, base/tool poses |

### Functions

#### `mr_dh_transform`
```c
mr_mat4 mr_dh_transform(const mr_dh_param *param);
```
Computes the 4×4 homogeneous transformation matrix for a single DH parameter set.

**Parameters:**
- `param` — DH parameters (a, alpha, d, theta)

**Returns:** 4×4 transformation matrix T_i^{i-1}

---

#### `mr_forward_kinematics`
```c
mr_pose mr_forward_kinematics(const mr_robot_desc *robot,
                              const double *q, int n);
```
Computes end-effector pose by multiplying all link transformations T = A1 × A2 × ... × An.

**Parameters:**
- `robot` — robot description with DH table
- `q` — joint positions array (length n)
- `n` — number of joints to consider

**Returns:** End-effector pose in world frame

---

#### `mr_forward_kinematics_chain`
```c
void mr_forward_kinematics_chain(const mr_robot_desc *robot,
                                 const double *q, int n,
                                 mr_mat4 *frames_out);
```
Computes transformation matrix of each link frame (useful for Jacobian).

**Parameters:**
- `robot` — robot description
- `q` — joint positions array
- `n` — number of joints
- `frames_out` — output array of size n, filled with link transforms

---

#### `mr_inverse_kinematics_numerical`
```c
int mr_inverse_kinematics_numerical(const mr_robot_desc *robot,
                                    const mr_pose *target,
                                    const double *q0, int n,
                                    double *q_out);
```
Numerical IK using damped least squares (Jacobian pseudo-inverse). Iterates up to `MR_MAX_IK_ITERS` or until error is below `MR_IK_TOL`.

**Parameters:**
- `robot` — robot description
- `target` — desired end-effector pose
- `q0` — initial guess for joint angles
- `n` — number of joints
- `q_out` — output joint solution

**Returns:** 1 if converged, 0 otherwise

---

#### `mr_inverse_kinematics_analytical`
```c
int mr_inverse_kinematics_analytical(const mr_robot_desc *robot,
                                     const mr_pose *target,
                                     int max_solutions,
                                     double solutions[][MR_MAX_JOINTS]);
```
Analytical IK for 3-DOF planar arms using geometric method (law of cosines).

**Returns:** Number of valid solutions found (0–2 typically)

---

#### `mr_build_jacobian`
```c
void mr_build_jacobian(const mr_robot_desc *robot,
                       const double *q, int n,
                       mr_jacobian *J);
```
Builds the geometric Jacobian at the current joint configuration.

---

#### `mr_jacobian_pinv`
```c
int mr_jacobian_pinv(const mr_jacobian *J, double damping,
                     double pinv[MR_MAX_JOINTS][6]);
```
Computes damped pseudo-inverse: J^T (J J^T + λ^2 I)^{-1}.

---

#### `mr_workspace_check`
```c
int mr_workspace_check(const mr_robot_desc *robot, const mr_pose *point);
```
Checks whether a point is within the robot's reachable workspace.

---

#### `mr_manipulability`
```c
double mr_manipulability(const mr_jacobian *J);
```
Yoshikawa manipulability measure: μ = sqrt(det(J J^T)). Higher values indicate better dexterity.

---

## path_planning.h

### Types

| Type | Description |
|------|-------------|
| `mr_grid_map` | 2D occupancy grid (width×height cells, resolution in m/cell) |
| `mr_path2d` | 2D path as sequence of waypoints |
| `mr_rrt_tree` | RRT data structure with nodes array |
| `mr_config2d` | 2D configuration: (x, y, theta) |
| `mr_world2d` | World model with obstacles and boundaries |
| `mr_trajectory` | Time-parameterized trajectory with velocity info |

### Functions

#### `mr_a_star`
```c
int mr_a_star(const mr_grid_map *map,
              const mr_point2d *start, const mr_point2d *goal,
              mr_path2d *path_out);
```
Finds shortest path on grid map using A* with Euclidean heuristic and 8-connected neighborhood.

#### `mr_dijkstra`
```c
int mr_dijkstra(const mr_grid_map *map,
                const mr_point2d *start, const mr_point2d *goal,
                mr_path2d *path_out);
```
Uniform cost search (Dijkstra's algorithm) on grid map.

#### `mr_rrt_plan`
```c
int mr_rrt_plan(mr_rrt_tree *tree,
                const mr_config2d *start, const mr_config2d *goal,
                int max_iter, mr_path2d *path_out);
```
Single-direction RRT planner. Samples random configurations in free space and extends tree toward them.

#### `mr_rrt_star_plan`
```c
int mr_rrt_star_plan(mr_rrt_tree *tree,
                     const mr_config2d *start, const mr_config2d *goal,
                     int max_iter, double radius, mr_path2d *path_out);
```
RRT* planner with optimal rewiring within `radius` — produces asymptotically optimal paths.

#### `mr_repulsive_potential`
```c
double mr_repulsive_potential(const mr_point2d *robot,
                              const mr_world2d *world,
                              double eta, double d0,
                              mr_point2d *force_out);
```
Computes repulsive potential field from obstacles. Returns total potential value and sets force vector.

---

## slam_system.h

### Types

| Type | Description |
|------|-------------|
| `mr_ekf_slam` | Extended Kalman Filter SLAM state |
| `mr_fast_slam` | Particle filter (FastSLAM) state |
| `mr_graph_slam` | Pose graph for graph-based SLAM |
| `mr_occ_grid` | Occupancy grid map |
| `mr_descriptor` | Scan descriptor for loop closure |

### Functions

#### `mr_ekf_slam_predict`
```c
void mr_ekf_slam_predict(mr_ekf_slam *slam, double dx, double dy, double dt);
```
Predict step: applies odometry motion model to state and covariance. Uses velocity motion model with Gaussian noise.

#### `mr_ekf_slam_update`
```c
int mr_ekf_slam_update(mr_ekf_slam *slam,
                       const mr_landmark_obs *observations,
                       int num_obs);
```
Update step: corrects estimated state using landmark range-bearing observations. Automatically initializes new landmarks.

#### `mr_fast_slam_predict`
```c
void mr_fast_slam_predict(mr_fast_slam *fs,
                          double dx, double dy, double dt,
                          double noise_std);
```
Propagates all particles through motion model with additive Gaussian noise.

#### `mr_fast_slam_resample`
```c
void mr_fast_slam_resample(mr_fast_slam *fs);
```
Systematic resampling of particles based on importance weights.

#### `mr_graph_slam_optimize`
```c
int mr_graph_slam_optimize(mr_graph_slam *gs, int max_iters);
```
Iterative Gauss-Seidel relaxation of pose graph constraints.

#### `mr_occ_grid_update_from_scan`
```c
void mr_occ_grid_update_from_scan(mr_occ_grid *grid,
                                  const mr_robot_pose *pose,
                                  const double *ranges, int num_ranges,
                                  double angle_min, double angle_inc,
                                  double max_range);
```
Updates occupancy grid from laser scan using simple hit-count model.

---

## ros_core.h

### Types

| Type | Description |
|------|-------------|
| `mr_master` | ROS master: topic registry |
| `mr_node` | ROS node: pub/sub, services, actions |
| `mr_message` | Serialized message with type definition |
| `mr_tf_tree` | TF2 transform tree |
| `mr_bag_writer` | Rosbag file writer |
| `mr_launch_file` | Launch file parser |

### Functions

#### `mr_node_subscribe`
```c
int mr_node_subscribe(mr_node *node, const char *topic_name,
                      mr_subscriber_cb cb);
```
Subscribes to a topic with a callback function.

#### `mr_node_publish`
```c
int mr_node_publish(mr_node *node, const char *topic_name,
                    const mr_message *msg);
```
Publishes a message to a topic.

#### `mr_bag_writer_open` / `mr_bag_writer_write` / `mr_bag_writer_close`
Bag file I/O with time-stamped message storage.

#### `mr_tf_tree_add_transform` / `mr_tf_tree_lookup`
Coordinate transform storage and retrieval for multi-frame robot systems.

---

## mobile_robot.h

### Types

| Type | Description |
|------|-------------|
| `mr_diff_drive_model` | Differential drive kinematic model |
| `mr_robot_state_2d` | 2D robot state (pose + velocity) |
| `mr_imu_data` | IMU sensor data with orientation |
| `mr_laser_scan` | 2D laser range finder data |
| `mr_amcl` | Adaptive Monte Carlo Localization state |
| `mr_pid_controller` | PID controller for velocity commands |

### Functions

#### `mr_odometry_update`
```c
void mr_odometry_update(mr_diff_drive_model *model,
                        double left_enc, double right_enc,
                        mr_robot_state_2d *state);
```
Updates robot pose from wheel encoder counts using differential drive kinematics.

#### `mr_imu_compute_orientation`
```c
void mr_imu_compute_orientation(mr_imu_data *imu, double dt);
```
Estimates roll/pitch from accelerometer and yaw from gyroscope (with magnetometer correction).

#### `mr_amcl_predict` / `mr_amcl_update_from_laser`
```c
void mr_amcl_predict(mr_amcl *amcl, double dx, double dy, double dtheta,
                     double alpha1, double alpha2,
                     double alpha3, double alpha4);
void mr_amcl_update_from_laser(mr_amcl *amcl,
                               const mr_laser_scan *scan,
                               const uint8_t *map, int map_w, int map_h,
                               double resolution,
                               double origin_x, double origin_y,
                               double likelihood_sigma);
```
Monte Carlo localization: predict step propagates particles, update step weights them by laser scan likelihood.

#### `mr_navigation_stack`
```c
void mr_navigation_stack(double current_x, double current_y,
                         double current_theta,
                         double goal_x, double goal_y,
                         const mr_laser_scan *scan,
                         double *cmd_linear, double *cmd_angular);
```
Combined navigation: go-to-goal behavior with laser-based obstacle avoidance override.

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `MR_MAX_JOINTS` | 16 | Maximum robot joints |
| `MR_MAX_IK_ITERS` | 500 | Max IK iterations |
| `MR_IK_TOL` | 1e-6 | IK convergence tolerance |
| `MR_PATH_MAX_WP` | 4096 | Max path waypoints |
| `MR_RRT_MAX_NODES` | 10000 | Max RRT tree nodes |
| `MR_SLAM_MAX_LANDMARKS` | 1024 | Max SLAM landmarks |
| `MR_SLAM_MAX_PARTICLES` | 256 | Max FastSLAM particles |
| `MR_AMCL_MAX_PARTICLES` | 4096 | Max AMCL particles |
| `MR_LASER_MAX_BEAMS` | 2048 | Max laser scan beams |
| `MR_ROS_MAX_TOPICS` | 256 | Max ROS topics |
| `MR_ROS_MAX_NODES` | 128 | Max ROS nodes |
