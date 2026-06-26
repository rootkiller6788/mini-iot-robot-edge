/*
 * bench_core.c - Core Benchmarks for mini-robotics
 *
 * Measures performance of the major API functions across all five sub-modules:
 *   kinematics.h, mobile_robot.h, path_planning.h, ros_core.h, slam_system.h
 *
 * Usage: bench_core [N]
 *   N = iteration scale factor (default 5000)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

#include "kinematics.h"
#include "mobile_robot.h"
#include "path_planning.h"
#include "ros_core.h"
#include "slam_system.h"

/* ---- helper: high-resolution timer ---- */
static double now_ms(void) {
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

/* ---- benchmark runner ---- */
static void bench_run(const char *name, void (*fn)(int), int n) {
    double t0 = now_ms();
    fn(n);
    double t1 = now_ms();
    double elapsed = t1 - t0;
    printf("  %-40s %d ops in %9.1f ms  (%8.1f us/op)\n",
           name, n, elapsed, (elapsed * 1000.0) / (double)n);
}

/* ================================================================
 *  BENCHMARKS -- kinematics.h
 * ================================================================ */

static void bm_dh_transform_mat4_chain(int n) {
    int scaled = n / 10; if (scaled < 1) scaled = 1;
    mr_dh_param dh = { 0.5, 0.1, 0.3, 0.2 };
    mr_pose pose = { 1.0, 2.0, 3.0, 0.1, 0.2, 0.3 };
    mr_mat4 mat_a, mat_b, mat_c;
    mr_pose pose_out;
    for (int i = 0; i < scaled; i++) {
        mat_a = mr_dh_transform(&dh);
        mr_pose_to_mat4(&pose, &mat_b);
        mat_c = mr_mat4_mul(&mat_a, &mat_b);
        mr_mat4_to_pose(&mat_c, &pose_out);
    }
}

static void bm_forward_kinematics(int n) {
    int scaled = n / 10; if (scaled < 1) scaled = 1;
    mr_robot_desc robot;
    memset(&robot, 0, sizeof(robot));
    robot.num_joints = 6;
    for (int j = 0; j < 6; j++) {
        robot.joints[j].type = MR_JOINT_REVOLUTE;
        robot.joints[j].dh.a = 0.5;
        robot.joints[j].dh.alpha = 0.1;
        robot.joints[j].dh.d = 0.3;
        robot.joints[j].dh.theta = 0.2 * (double)(j + 1);
        robot.joints[j].q_min = -M_PI;
        robot.joints[j].q_max = M_PI;
    }
    double q[6] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6 };
    for (int i = 0; i < scaled; i++) {
        mr_forward_kinematics(&robot, q, 6);
    }
}

static void bm_inverse_kinematics_jacobian(int n) {
    int scaled = n / 50; if (scaled < 1) scaled = 1;
    mr_robot_desc robot;
    memset(&robot, 0, sizeof(robot));
    robot.num_joints = 6;
    for (int j = 0; j < 6; j++) {
        robot.joints[j].type = MR_JOINT_REVOLUTE;
        robot.joints[j].dh.a = 0.5;
        robot.joints[j].dh.alpha = 0.1;
        robot.joints[j].dh.d = 0.3;
        robot.joints[j].q_min = -M_PI;
        robot.joints[j].q_max = M_PI;
    }
    mr_pose target = { 1.5, 0.5, 0.8, 0.0, 0.5, 0.0 };
    double q0[6] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6 };
    double q_out[6];
    mr_jacobian J;
    double pinv_j[MR_MAX_JOINTS][6];
    for (int i = 0; i < scaled; i++) {
        mr_inverse_kinematics_numerical(&robot, &target, q0, 6, q_out);
        mr_build_jacobian(&robot, q_out, 6, &J);
        mr_jacobian_pinv(&J, 0.01, pinv_j);
        mr_manipulability(&J);
    }
}

static void bm_forward_kinematics_chain(int n) {
    int scaled = n / 20; if (scaled < 1) scaled = 1;
    mr_robot_desc robot;
    memset(&robot, 0, sizeof(robot));
    robot.num_joints = 6;
    for (int j = 0; j < 6; j++) {
        robot.joints[j].type = MR_JOINT_REVOLUTE;
        robot.joints[j].dh.a = 0.5;
        robot.joints[j].dh.alpha = 0.1;
        robot.joints[j].dh.d = 0.3;
        robot.joints[j].q_min = -M_PI;
        robot.joints[j].q_max = M_PI;
    }
    double q[6] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6 };
    mr_mat4 frames[MR_MAX_JOINTS];
    for (int i = 0; i < scaled; i++) {
        mr_forward_kinematics_chain(&robot, q, 6, frames);
    }
}

/* ================================================================
 *  BENCHMARKS -- mobile_robot.h  (Diff Drive, Odometry, IMU, Laser, AMCL, PID)
 * ================================================================ */

static void bm_diff_drive_odometry(int n) {
    int scaled = n / 10; if (scaled < 1) scaled = 1;
    mr_diff_drive_model model;
    mr_robot_state_2d state;
    memset(&state, 0, sizeof(state));
    mr_wheel_velocities wheels = { 0.5, 0.5 };
    mr_body_velocity body;
    mr_robot_pose_2d pose;
    for (int i = 0; i < scaled; i++) {
        mr_diff_drive_setup(&model, 0.15, 0.05, 1.0);
        mr_diff_drive_wheel_to_body(&model, &wheels, &body);
        mr_diff_drive_body_to_wheel(&model, &body, &wheels);
        mr_odometry_update(&model, (double)i, (double)i, &state);
        mr_odometry_get_pose(&state, &pose);
    }
}

static void bm_imu_init_update(int n) {
    int scaled = n / 10; if (scaled < 1) scaled = 1;
    mr_imu_data imu;
    double roll, pitch, yaw;
    for (int i = 0; i < scaled; i++) {
        mr_imu_init(&imu);
        mr_imu_update_accel(&imu, 0.01, 0.02, 9.81);
        mr_imu_update_gyro(&imu, 0.001, 0.002, 0.003);
        mr_imu_update_mag(&imu, 30.0, 20.0, 40.0);
        mr_imu_compute_orientation(&imu, 0.01);
        mr_imu_get_rpy(&imu, &roll, &pitch, &yaw);
    }
}

static void bm_laser_scan_init(int n) {
    int scaled = n / 20; if (scaled < 1) scaled = 1;
    mr_laser_scan scan;
    for (int i = 0; i < scaled; i++) {
        mr_laser_scan_init(&scan, -M_PI, M_PI, M_PI / 180.0, 0.1, 12.0);
        for (int k = 0; k < 360; k++) {
            mr_laser_scan_set_beam(&scan, k, 2.0 + (k % 10) * 0.1);
        }
        mr_laser_scan_min_range(&scan);
        mr_laser_scan_detect_obstacle(&scan, 0.5);
    }
}

static void bm_amcl_init_predict_update(int n) {
    int scaled = n / 50; if (scaled < 1) scaled = 1;
    mr_amcl amcl;
    double x, y, theta;
    for (int i = 0; i < scaled; i++) {
        mr_amcl_init(&amcl, 1000, -10.0, 10.0, -10.0, 10.0);
        mr_amcl_predict(&amcl, 0.1, 0.0, 0.01, 0.1, 0.1, 0.05, 0.05);
        mr_amcl_resample(&amcl);
        mr_amcl_get_estimate(&amcl, &x, &y, &theta);
    }
}

static void bm_pid_compute_nav(int n) {
    int scaled = n / 10; if (scaled < 1) scaled = 1;
    mr_pid_controller pid;
    double cmd_lin, cmd_ang;
    for (int i = 0; i < scaled; i++) {
        mr_pid_init(&pid, 1.0, 0.1, 0.05, 2.0, 0.2, 0.1, 1.0, M_PI);
        mr_pid_compute(&pid, 0.5, 0.3, 0.4, 0.25, 0.01, &cmd_lin, &cmd_ang);
        mr_goto_goal(0.0, 0.0, 0.0, 5.0, 5.0, &cmd_lin, &cmd_ang);
    }
}

/* ================================================================
 *  BENCHMARKS -- path_planning.h
 * ================================================================ */

static void bm_grid_map_heuristic(int n) {
    int scaled = n / 10; if (scaled < 1) scaled = 1;
    mr_grid_map map;
    mr_point2d a = { 0, 0 }, b = { 99, 99 };
    int gx, gy;
    for (int i = 0; i < scaled; i++) {
        mr_grid_map_init(&map, 100, 100, 0.05, 0.0, 0.0);
        mr_grid_map_set_cell(&map, i % 100, (i + 50) % 100, 0);
        mr_grid_map_get_cell(&map, i % 100, (i + 50) % 100);
        mr_grid_map_world_to_grid(&map, 1.0, 2.0, &gx, &gy);
        mr_heuristic_euclidean(&a, &b);
        mr_heuristic_manhattan(&a, &b);
    }
}

static void bm_a_star_dijkstra_rrt(int n) {
    int scaled = n / 50; if (scaled < 1) scaled = 1;
    mr_grid_map map;
    mr_grid_map_init(&map, 100, 100, 0.05, 0.0, 0.0);
    mr_point2d start = { 1, 1 }, goal = { 98, 98 };
    mr_path2d path_a, path_d, path_r;
    mr_world2d world;
    memset(&world, 0, sizeof(world));
    world.width = 10.0; world.height = 10.0;
    mr_config2d cfg_start = { 1.0, 1.0, 0.0 };
    mr_config2d cfg_goal = { 9.0, 9.0, 0.0 };
    mr_rrt_tree tree;
    mr_trajectory traj;
    for (int i = 0; i < scaled; i++) {
        mr_a_star(&map, &start, &goal, &path_a);
        mr_dijkstra(&map, &start, &goal, &path_d);
        mr_rrt_init(&tree, &world, 0.1, 0.05);
        mr_rrt_plan(&tree, &cfg_start, &cfg_goal, 500, &path_r);
        mr_rrt_star_plan(&tree, &cfg_start, &cfg_goal, 500, 1.5, &path_r);
        mr_rrt_smooth_path(&path_a, &path_d);
        mr_trajectory_from_waypoints(&path_a, 0.5, 0.2, &traj);
    }
}

static void bm_collision_potential(int n) {
    int scaled = n / 10; if (scaled < 1) scaled = 1;
    mr_world2d world;
    memset(&world, 0, sizeof(world));
    world.width = 10.0; world.height = 10.0;
    mr_point2d robot = { 2.0, 3.0 };
    mr_point2d force;
    for (int i = 0; i < scaled; i++) {
        mr_collision_check_point(&robot, &world);
        mr_repulsive_potential(&robot, &world, 0.5, 2.0, &force);
    }
}

/* ================================================================
 *  BENCHMARKS -- ros_core.h
 * ================================================================ */

static void bm_master_node_pub_sub(int n) {
    int scaled = n / 10; if (scaled < 1) scaled = 1;
    mr_master master;
    mr_node node;
    mr_message msg;
    mr_msg_definition def;
    memset(&def, 0, sizeof(def));
    for (int i = 0; i < scaled; i++) {
        mr_master_init(&master);
        mr_master_register_topic(&master, "/cmd_vel", "geometry_msgs/Twist");
        mr_node_init(&node, "controller");
        mr_node_subscribe(&node, "/scan", NULL);
        mr_message_init(&msg, &def);
        mr_message_set_float32(&msg, 0, 3.14f);
        mr_message_get_float32(&msg, 0);
        mr_node_publish(&node, "/cmd_vel", &msg);
    }
}

static void bm_bag_tf_launch(int n) {
    int scaled = n / 50; if (scaled < 1) scaled = 1;
    mr_bag_writer bag;
    mr_tf_tree tf;
    double trans[3] = { 0.1, 0.0, 0.0 };
    double rot[4] = { 0.0, 0.0, 0.0, 1.0 };
    double trans_out[3], rot_out[4];
    mr_launch_file lf;
    for (int i = 0; i < scaled; i++) {
        mr_bag_writer_open(&bag, "recording.bag");
        mr_bag_writer_close(&bag);
        mr_tf_tree_init(&tf);
        mr_tf_tree_add_transform(&tf, "base_link", "laser", trans, rot);
        mr_tf_tree_lookup(&tf, "base_link", "laser", trans_out, rot_out);
        mr_launch_file_parse("<launch></launch>", &lf);
    }
}

/* ================================================================
 *  BENCHMARKS -- slam_system.h
 * ================================================================ */

static void bm_ekf_slam(int n) {
    int scaled = n / 100; if (scaled < 1) scaled = 1;
    mr_ekf_slam slam;
    mr_robot_pose pose;
    mr_landmark_obs obs[2];
    for (int i = 0; i < scaled; i++) {
        mr_ekf_slam_init(&slam);
        mr_ekf_slam_predict(&slam, 0.1, 0.0, 0.01);
        obs[0].range = 2.0; obs[0].bearing = 0.5; obs[0].id = 1;
        obs[1].range = 3.0; obs[1].bearing = -0.3; obs[1].id = 2;
        mr_ekf_slam_update(&slam, obs, 2);
        mr_ekf_slam_get_pose(&slam, &pose);
        mr_ekf_slam_add_landmark(&slam, 3, 4.0, 0.8);
    }
}

static void bm_fast_slam(int n) {
    int scaled = n / 100; if (scaled < 1) scaled = 1;
    mr_fast_slam fs;
    mr_robot_pose pose;
    mr_landmark_obs obs[1];
    for (int i = 0; i < scaled; i++) {
        mr_fast_slam_init(&fs, 100);
        mr_fast_slam_predict(&fs, 0.05, 0.0, 0.005, 0.1);
        obs[0].range = 2.0; obs[0].bearing = 0.3; obs[0].id = 1;
        mr_fast_slam_update(&fs, obs, 1);
        mr_fast_slam_resample(&fs);
        mr_fast_slam_best_pose(&fs, &pose);
    }
}

static void bm_graph_slam(int n) {
    int scaled = n / 100; if (scaled < 1) scaled = 1;
    mr_graph_slam gs;
    mr_robot_pose initial = { 0, 0, 0 };
    mr_robot_pose p2 = { 0.1, 0.0, 0.01 };
    for (int i = 0; i < scaled; i++) {
        mr_graph_slam_init(&gs, &initial);
        mr_graph_slam_add_node(&gs, &initial);
        mr_graph_slam_add_node(&gs, &p2);
        mr_graph_slam_add_edge(&gs, 0, 1, 0.1, 0.0, 0.01);
        mr_graph_slam_optimize(&gs, 10);
        mr_graph_slam_loop_closure(&gs, 0, 1, 0.1, 0.0, 0.01);
    }
}

static void bm_occ_grid(int n) {
    int scaled = n / 20; if (scaled < 1) scaled = 1;
    mr_occ_grid grid;
    mr_robot_pose pose = { 1.0, 2.0, 0.2 };
    double ranges[360];
    for (int k = 0; k < 360; k++) ranges[k] = 5.0 + (k % 10) * 0.5;
    int valid;
    for (int i = 0; i < scaled; i++) {
        mr_occ_grid_init(&grid, 200, 200, 0.05, 0.0, 0.0);
        mr_occ_grid_update_from_scan(&grid, &pose, ranges, 360,
            -M_PI, M_PI / 180.0, 12.0);
        mr_occ_grid_get(&grid, 100, 100, &valid);
        mr_occ_grid_occupied(&grid, 5.0, 5.0);
    }
}

/* ================================================================
 *  main
 * ================================================================ */

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 5000;
    if (N < 1) N = 5000;

    printf("=== mini-robotics Benchmarks (N=%d) ===\n\n", N);

    /* kinematics.h */
    bench_run("DH transform + mat4 chain",              bm_dh_transform_mat4_chain, N);
    bench_run("Forward kinematics (6-DOF)",              bm_forward_kinematics, N);
    bench_run("Inverse kinematics + Jacobian + manip",  bm_inverse_kinematics_jacobian, N);
    bench_run("Forward kinematics chain (6 frames)",    bm_forward_kinematics_chain, N);

    /* mobile_robot.h */
    bench_run("Diff drive + odometry",                  bm_diff_drive_odometry, N);
    bench_run("IMU init + update + compute + RPY",      bm_imu_init_update, N);
    bench_run("Laser scan init + 360 beams",            bm_laser_scan_init, N);
    bench_run("AMCL init + predict + resample",         bm_amcl_init_predict_update, N);
    bench_run("PID compute + goto_goal",                bm_pid_compute_nav, N);

    /* path_planning.h */
    bench_run("Grid map + heuristic",                   bm_grid_map_heuristic, N);
    bench_run("A* + Dijkstra + RRT + trajectory",       bm_a_star_dijkstra_rrt, N);
    bench_run("Collision check + potential field",      bm_collision_potential, N);

    /* ros_core.h */
    bench_run("Master + node + pub/sub",                bm_master_node_pub_sub, N);
    bench_run("Bag writer + TF tree + launch file",     bm_bag_tf_launch, N);

    /* slam_system.h */
    bench_run("EKF SLAM init + predict + update",       bm_ekf_slam, N);
    bench_run("FastSLAM init + predict + resample",     bm_fast_slam, N);
    bench_run("Graph SLAM add node + edge + optimize",  bm_graph_slam, N);
    bench_run("Occupancy grid init + scan update",      bm_occ_grid, N);

    printf("\nDone.\n");
    return 0;
}
