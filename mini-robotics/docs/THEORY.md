# mini-robotics — Theory Reference

## 1. Kinematics

### 1.1 Denavit-Hartenberg (DH) Convention

The DH convention assigns a coordinate frame to each link of a serial manipulator
using four parameters per joint:

- **a_i** — link length: distance from z_{i-1} to z_i along x_i
- **α_i** — link twist: angle from z_{i-1} to z_i about x_i
- **d_i** — link offset: distance from x_{i-1} to x_i along z_{i-1}
- **θ_i** — joint angle: angle from x_{i-1} to x_i about z_{i-1}

The transformation from frame i-1 to frame i is:

```
T_i^{i-1} = Rot(z, θ) · Trans(z, d) · Trans(x, a) · Rot(x, α)

          = | cos θ   -sin θ cos α   sin θ sin α   a cos θ |
            | sin θ    cos θ cos α  -cos θ sin α   a sin θ |
            | 0        sin α         cos α          d       |
            | 0        0             0              1       |
```

### 1.2 Forward Kinematics

The end-effector pose in the base frame is computed by chaining all link transforms:

```
T_n^0 = T_1^0 · T_2^1 · ... · T_n^{n-1}
```

This is a 4×4 homogeneous transformation matrix. Position is extracted from
column 4, orientation from the 3×3 rotation sub-matrix.

### 1.3 Inverse Kinematics

**Numerical (Jacobian Inverse):**

The Jacobian J relates joint velocities q_dot to end-effector twist:

```
[v; ω] = J(q) · q_dot
```

For IK, we use the damped pseudo-inverse:

```
Δq = J^T (J J^T + λ^2 I)^{-1} · e
```

where e is the 6-DOF pose error vector. This is iterated until convergence.

**Analytical (Planar 3-DOF):**

For a planar arm with links L1, L2, the law of cosines gives:

```
cos(θ2) = (r² - L1² - L2²) / (2 · L1 · L2)
```

where r is the distance from shoulder to wrist in the plane.

### 1.4 Manipulability

Yoshikawa's manipulability measure:

```
μ = √(det(J · J^T))
```

Higher μ means the arm can move more freely in all directions. Near singularities, μ → 0.

### 1.5 Workspace

The reachable workspace is the set of all points the end-effector can reach given joint limits.
It is approximated by sampling joint configurations at limit extremes.

---

## 2. Path Planning

### 2.1 A* Search

A* finds the shortest path on a grid by expanding nodes with minimum:

```
f(n) = g(n) + h(n)
```

where:
- **g(n)** — actual cost from start to node n
- **h(n)** — heuristic estimate from n to goal (must be admissible)

Euclidean distance: h(n) = √((x_n - x_goal)² + (y_n - y_goal)²)

Manhattan distance: h(n) = |x_n - x_goal| + |y_n - y_goal|

### 2.2 Dijkstra's Algorithm

Special case of A* with h(n) = 0 for all n. Guarantees shortest path but explores more nodes.

### 2.3 RRT (Rapidly-exploring Random Tree)

**Algorithm:**
1. Initialize tree with start configuration
2. Sample random configuration in free space
3. Find nearest node in tree
4. Extend from nearest toward sample by step size
5. If extension is collision-free, add to tree
6. Repeat until goal is reached or max iterations

Uses goal biasing: with probability p, sample the goal directly.

### 2.4 RRT*

Extension of RRT that produces asymptotically optimal paths:

**Rewiring step:**
After adding a new node:
1. Find all nodes within radius r of the new node
2. Reassign parent if a better path exists through the new node
3. Rewire neighbors if the new node provides a better path

### 2.5 Repulsive Potential Field

The repulsive potential from an obstacle at distance d is:

```
U_rep(d) = { ½ η (1/d - 1/d₀)²   if d ≤ d₀
           { 0                     if d > d₀
```

The corresponding force is the negative gradient:

```
F_rep = -∇U_rep = η (1/d - 1/d₀) · (1/d²) · (∂d/∂x, ∂d/∂y)
```

### 2.6 Trajectory Generation

**Trapezoidal velocity profile:** Accelerate to max velocity, cruise, then decelerate.
```
v(t) = { a·t               for t ∈ [0, T_a]
       { v_max             for t ∈ [T_a, T_a + T_c]
       { v_max - a·(t-T_c) for t ∈ [T_a+T_c, 2T_a+T_c]
```

**Cubic spline:** Linear interpolation between waypoints with parameterization by arc length.

---

## 3. Simultaneous Localization and Mapping (SLAM)

### 3.1 EKF-SLAM

The state vector contains the robot pose and all landmark positions:

```
x = [x_r, y_r, θ_r, lx_1, ly_1, ..., lx_N, ly_N]^T
```

**Predict step:** Uses odometry motion model to update robot pose and Jacobians.

```
x_{t+1} = g(x_t, u_t) + ε     (ε ~ N(0, Q))
Σ_{t+1} = G Σ_t G^T + R
```

**Update step:** For each landmark observation (range r, bearing φ):

Expected measurement:
```
ẑ = [ √((lx - x_r)² + (ly - y_r)²) ]
    [ atan2(ly - y_r, lx - x_r) - θ_r ]
```

Innovation: y = z - ẑ

Kalman gain: K = Σ H^T (H Σ H^T + R)^{-1}

State update: x = x + K y

Covariance: Σ = (I - K H) Σ

### 3.2 FastSLAM (Particle Filter)

FastSLAM factorizes the SLAM posterior:

```
p(x_{1:t}, m | z_{1:t}, u_{1:t-1})
   = p(x_{1:t} | z_{1:t}, u_{1:t-1}) · ∏_{k=1}^K p(m_k | x_{1:t}, z_{1:t})
```

Each particle maintains its own map of landmarks (EKFs per landmark).

**Resampling:** Particles with low weight are replaced by copies of high-weight particles.
This combats particle depletion while maintaining diversity.

### 3.3 Graph SLAM

The SLAM problem is represented as a graph:
- **Nodes** = robot poses at different times
- **Edges** = spatial constraints between poses (odometry or loop closures)

**Optimization objective:**

```
X* = argmin_X Σ e_ij^T Ω_ij e_ij
```

where e_ij = z_ij - h(x_i, x_j) is the constraint error and Ω_ij is the information matrix.

Gauss-Seidel relaxation iteratively adjusts each node:

```
x_i += α Σ_j Ω_ij (z_ij - h(x_i, x_j))
```

### 3.4 Loop Closure Detection

Loop closures are detected by matching laser scan descriptors.
A simple descriptor binarizes range readings at fixed angular intervals.
Similarity between two descriptors is:

```
s(a, b) = 1 / (1 + √(Σ (a_i - b_i)² / n))
```

When similarity exceeds a threshold, a new edge is added to the pose graph.

### 3.5 Occupancy Grid Mapping

Each cell stores an occupancy probability (0 = free, 255 = occupied, 127 = unknown).
For each laser beam, cells along the ray are marked free, and the endpoint cell is marked occupied:

```
P(occ | z) = update using log-odds
l_{t+1} = l_t + log(P(z|occ) / (1 - P(z|occ)))
```

The simpler hit-count model increments occupied cells by a fixed amount.

---

## 4. ROS Communication Model

### 4.1 Topic (Publish/Subscribe)

- **Publisher** declares a topic name and message type
- **Subscriber** registers callback for a topic
- **Master** maintains the topic registry, matching publishers to subscribers
- Messages are serialized to binary (custom format in this implementation)

### 4.2 Service (Request/Response)

Synchronous RPC pattern:
1. Client sends request message
2. Server processes and sends response
3. Client blocks until response arrives

### 4.3 Action (Goal/Feedback/Result)

Asynchronous pattern for long-running tasks:
1. Client sends goal
2. Server provides periodic feedback
3. Server signals completion with result
4. Client can cancel at any time

### 4.4 Bag File Format

```
[timestamp (uint64)] [topic_len (uint32)] [topic_name] [msg_len (uint32)] [msg_data]
```

Repeated for each recorded message.

### 4.5 TF2 Transform Tree

Directed graph of coordinate frames. Each edge stores:
- Parent frame name
- Child frame name
- Translation (x, y, z)
- Rotation (quaternion: x, y, z, w)

Lookup chain: traverse from child to parent, multiply transforms.

---

## 5. Mobile Robot Control

### 5.1 Differential Drive Kinematics

Given wheel velocities (v_L, v_R), wheel radius r, and track width L:

```
v = r · (v_R + v_L) / 2        (linear velocity)
ω = r · (v_R - v_L) / L        (angular velocity)
```

Inverse (from body to wheels):
```
v_L = (v - ωL/2) / r
v_R = (v + ωL/2) / r
```

### 5.2 Odometry

From encoder counts:

```
Δs_L = 2πr · Δenc_L / CPR      (left wheel displacement)
Δs_R = 2πr · Δenc_R / CPR      (right wheel displacement)
Δs = (Δs_L + Δs_R) / 2         (center displacement)
Δθ = (Δs_R - Δs_L) / L         (heading change)

x' = x + Δs · cos(θ + Δθ/2)
y' = y + Δs · sin(θ + Δθ/2)
θ' = θ + Δθ
```

### 5.3 IMU Orientation

**Gyroscope integration:** θ' = θ + ω · Δt (short-term accurate, long-term drift)

**Accelerometer:** tilt angles from gravity vector:
```
roll  = atan2(a_y, a_z)
pitch = atan2(-a_x, √(a_y² + a_z²))
```

**Magnetometer:** heading from horizontal field components:
```
yaw = atan2(m_y, m_x)
```

Complementary filter: gyro for fast dynamics + accel/mag for low-frequency correction.

### 5.4 AMCL (Monte Carlo Localization)

**Particles:** Set of weighted hypotheses (x, y, θ, w).

**Motion model (odometry with noise):**
```
x' = x + Δs · cos(θ + Δθ/2) + ε_x
y' = y + Δs · sin(θ + Δθ/2) + ε_y
θ' = θ + Δθ + ε_θ
```

**Sensor model (likelihood field):**
For each laser beam, find the nearest occupied cell in the map.
Weight = Π Gaussian(beam_endpoint, closest_obstacle, σ²).

**Resampling:** Low variance systematic resampling.

### 5.5 PID Control

```
u(t) = K_p e(t) + K_i ∫e(t)dt + K_d de/dt

Linear:  cmd_linear  = Kp·e_lin  + Ki·∑e_lin·dt  + Kd·(e_lin - e_prev)/dt
Angular: cmd_angular = Kp·e_ang  + Ki·∑e_ang·dt  + Kd·(e_ang - e_prev)/dt
```

### 5.6 Go-to-Goal Controller

```
dist = √((gx - x)² + (gy - y)²)
angle_error = atan2(gy - y, gx - x) - θ

v = K_lin · dist      (proportional to distance)
ω = K_ang · angle_error
```

### 5.7 Obstacle Avoidance

Laser-based reactive avoidance:
1. Scan left and right halves of laser readings
2. If any beam < safe_distance, accumulate error
3. Turn away from the side with more nearby obstacles
4. Blend with go-to-goal command

### 5.8 Wall Following

Maintain a fixed distance from the wall on the right side:
```
error = wall_distance - desired_distance
ω_correction = -K_p · error
```

---

## References

1. Craig, J. J. (2005). *Introduction to Robotics: Mechanics and Control.* Pearson.
2. Siciliano, B. et al. (2010). *Robotics: Modelling, Planning and Control.* Springer.
3. Thrun, S., Burgard, W., & Fox, D. (2005). *Probabilistic Robotics.* MIT Press.
4. LaValle, S. M. (2006). *Planning Algorithms.* Cambridge University Press.
5. Karaman, S. & Frazzoli, E. (2011). "Sampling-based algorithms for optimal motion planning." *IJRR*.
6. Quigley, M. et al. (2009). "ROS: an open-source Robot Operating System." *ICRA Workshop*.
7. Grisetti, G. et al. (2010). "A Tutorial on Graph-Based SLAM." *IEEE ITS Magazine*.
8. Fox, D. et al. (1999). "Monte Carlo Localization: Efficient Position Estimation for Mobile Robots." *AAAI*.
