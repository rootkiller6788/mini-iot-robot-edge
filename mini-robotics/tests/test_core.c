/*
 * test_core.c - Core Unit Tests for mini-robotics
 *
 * Tests all five sub-modules:
 *   kinematics.h, mobile_robot.h, path_planning.h, ros_core.h, slam_system.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

#include "kinematics.h"
#include "mobile_robot.h"
#include "path_planning.h"
#include "ros_core.h"
#include "slam_system.h"

/* ---- test harness ---- */
static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s ... ", name); } while(0)
#define PASS()     do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg)  do { printf("FAIL: %s\n", msg); return 1; } while(0)
#define CHECK(cond, msg) if (!(cond)) FAIL(msg)

/* ================================================================
 *  kinematics.h  (DH transform, forward/inverse kinematics, Jacobian)
 * ================================================================ */

static int test_dh_transform(void) {
    TEST("mr_dh_transform");
    mr_dh_param dh = { 0.5, 0.1, 0.3, 0.2 };
    mr_mat4 T = mr_dh_transform(&dh);
    CHECK(fabs(T.m[3][3] - 1.0) < 0.001, "homogeneous matrix bottom-right should be 1.0");
    PASS();
    return 0;
}

static int test_mat4_identity_inverse(void) {
    TEST("mr_mat4_identity / mr_mat4_inv");
    mr_mat4 I = mr_mat4_identity();
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (r == c) CHECK(fabs(I.m[r][c] - 1.0) < 0.001, "identity diagonal should be 1");
            else        CHECK(fabs(I.m[r][c] - 0.0) < 0.001, "identity off-diagonal should be 0");
    mr_mat4 inv = mr_mat4_inv(&I);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (r == c) CHECK(fabs(inv.m[r][c] - 1.0) < 0.001, "I^-1 should equal I");
    PASS();
    return 0;
}

static int test_mat4_multiply(void) {
    TEST("mr_mat4_mul");
    mr_mat4 A = mr_mat4_identity();
    mr_mat4 B = mr_mat4_identity();
    mr_mat4 C = mr_mat4_mul(&A, &B);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (r == c) CHECK(fabs(C.m[r][c] - 1.0) < 0.001, "I*I should match I");
    PASS();
    return 0;
}

static int test_pose_mat4_conversion(void) {
    TEST("mr_pose_to_mat4 / mr_mat4_to_pose round-trip");
    mr_pose orig = { 1.0, 2.0, 3.0, 0.1, 0.2, 0.3 };
    mr_mat4 m;
    mr_pose_to_mat4(&orig, &m);
    mr_pose back;
    mr_mat4_to_pose(&m, &back);
    double dist = mr_pose_distance(&orig, &back);
    CHECK(dist < 0.001, "pose -> mat4 -> pose should round-trip");
    PASS();
    return 0;
}

static int test_forward_kinematics(void) {
    TEST("mr_forward_kinematics (6-DOF)");
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
    mr_pose pose = mr_forward_kinematics(&robot, q, 6);
    CHECK(isfinite(pose.x) && isfinite(pose.y) && isfinite(pose.z),
          "FK pose should have finite coordinates");
    PASS();
    return 0;
}

static int test_inverse_kinematics(void) {
    TEST("mr_inverse_kinematics_numerical convergence");
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
    double q_init[6] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6 };
    mr_pose target = mr_forward_kinematics(&robot, q_init, 6);
    double q_sol[6];
    int rc = mr_inverse_kinematics_numerical(&robot, &target, q_init, 6, q_sol);
    CHECK(rc >= 0, "IK returned error code");
    mr_pose reached = mr_forward_kinematics(&robot, q_sol, 6);
    double dist = mr_pose_distance(&target, &reached);
    CHECK(dist < 0.1, "IK solution should converge to target");
    PASS();
    return 0;
}

static int test_jacobian_manipulability(void) {
    TEST("mr_build_jacobian / mr_jacobian_pinv / mr_manipulability");
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
    mr_jacobian J;
    mr_build_jacobian(&robot, q, 6, &J);
    CHECK(J.cols > 0, "Jacobian should have valid columns");
    double pinv_arr[MR_MAX_JOINTS][6];
    int rc = mr_jacobian_pinv(&J, 0.01, pinv_arr);
    CHECK(rc == 0, "pseudo-inverse should succeed");
    double manip = mr_manipulability(&J);
    CHECK(manip >= 0.0, "manipulability should be non-negative");
    PASS();
    return 0;
}

/* ================================================================
 *  mobile_robot.h  (Diff Drive, Odometry, IMU, Laser, AMCL, PID)
 * ================================================================ */

static int test_diff_drive_odometry(void) {
    TEST("mr_diff_drive_setup + odometry round-trip");
    mr_diff_drive_model model;
    mr_diff_drive_setup(&model, 0.15, 0.05, 1.0);
    CHECK(fabs(model.wheel_radius - 0.15) < 0.001, "wheel radius should be 0.15");
    CHECK(fabs(model.track_width - 0.05) < 0.001, "track width should be 0.05");

    mr_wheel_velocities wheels = { 0.5, 0.5 };
    mr_body_velocity body;
    mr_diff_drive_wheel_to_body(&model, &wheels, &body);
    mr_wheel_velocities wheels2;
    mr_diff_drive_body_to_wheel(&model, &body, &wheels2);
    CHECK(fabs(wheels.left_velocity - wheels2.left_velocity) < 0.01,
          "wheel->body->wheel should round-trip");

    mr_robot_state_2d state;
    memset(&state, 0, sizeof(state));
    mr_odometry_update(&model, 100.0, 100.0, &state);
    mr_robot_pose_2d pose;
    mr_odometry_get_pose(&state, &pose);
    CHECK(pose.x > 0.0, "odometry should advance x position");
    double linear, angular;
    mr_odometry_get_velocity(&model, 0.01, &state, &linear, &angular);
    CHECK(isfinite(linear) && isfinite(angular), "velocity should be finite");
    PASS();
    return 0;
}

static int test_imu_init_update(void) {
    TEST("mr_imu_init + update accel/gyro/mag + compute orientation");
    mr_imu_data imu;
    mr_imu_init(&imu);
    mr_imu_update_accel(&imu, 0.0, 0.0, 9.81);
    mr_imu_update_gyro(&imu, 0.01, 0.02, 0.03);
    mr_imu_update_mag(&imu, 30.0, 20.0, 40.0);
    mr_imu_compute_orientation(&imu, 0.01);
    double roll, pitch, yaw;
    mr_imu_get_rpy(&imu, &roll, &pitch, &yaw);
    CHECK(isfinite(roll) && isfinite(pitch) && isfinite(yaw), "RPY should be finite");
    PASS();
    return 0;
}

static int test_laser_scan(void) {
    TEST("mr_laser_scan_init + set_beam + min_range + detect_obstacle");
    mr_laser_scan scan;
    mr_laser_scan_init(&scan, -M_PI, M_PI, M_PI / 180.0, 0.1, 12.0);
    CHECK(scan.num_beams > 0, "laser scan should have beams");
    mr_laser_scan_set_beam(&scan, 0, 3.5);
    mr_laser_scan_set_beam(&scan, 100, 8.2);
    double min_r = mr_laser_scan_min_range(&scan);
    CHECK(isfinite(min_r), "min range should be finite");
    int obstacles = mr_laser_scan_detect_obstacle(&scan, 0.5);
    CHECK(obstacles >= 0, "detect_obstacle should return valid count");
    PASS();
    return 0;
}

static int test_amcl(void) {
    TEST("mr_amcl_init / predict / resample / get_estimate / covariance");
    mr_amcl amcl;
    mr_amcl_init(&amcl, 500, -5.0, 5.0, -5.0, 5.0);
    CHECK(amcl.num_particles == 500, "should have 500 particles");
    mr_amcl_predict(&amcl, 0.1, 0.0, 0.01, 0.1, 0.1, 0.05, 0.05);
    mr_amcl_resample(&amcl);
    double x, y, theta;
    mr_amcl_get_estimate(&amcl, &x, &y, &theta);
    CHECK(isfinite(x) && isfinite(y) && isfinite(theta), "estimate should be finite");
    double cxx, cyy, ctt, cxy, cxt, cyt;
    mr_amcl_get_covariance(&amcl, &cxx, &cyy, &ctt, &cxy, &cxt, &cyt);
    CHECK(cxx >= 0.0 && cyy >= 0.0, "covariance diagonals should be >= 0");
    PASS();
    return 0;
}

static int test_pid_navigation(void) {
    TEST("mr_pid_init / compute + mr_goto_goal");
    mr_pid_controller pid;
    mr_pid_init(&pid, 1.0, 0.1, 0.05, 2.0, 0.2, 0.1, 1.0, M_PI);
    CHECK(fabs(pid.kp_linear - 1.0) < 0.001, "kp_linear should be 1.0");

    double cmd_lin, cmd_ang;
    mr_pid_compute(&pid, 0.5, 0.3, 0.0, 0.0, 0.01, &cmd_lin, &cmd_ang);
    CHECK(isfinite(cmd_lin) && isfinite(cmd_ang), "PID output should be finite");
    CHECK(cmd_lin > 0.0, "should command positive forward velocity");

    mr_goto_goal(0.0, 0.0, 0.0, 5.0, 0.0, &cmd_lin, &cmd_ang);
    CHECK(cmd_lin > 0.0, "goto_goal should drive forward");
    PASS();
    return 0;
}

/* ================================================================
 *  path_planning.h
 * ================================================================ */

static int test_grid_map_heuristic(void) {
    TEST("mr_grid_map_init + set/get cell + world<->grid + heuristics");
    mr_grid_map map;
    mr_grid_map_init(&map, 200, 200, 0.05, -5.0, -5.0);
    CHECK(map.width == 200 && map.height == 200, "grid map should be 200x200");

    mr_grid_map_set_cell(&map, 10, 20, 255);
    uint8_t v = mr_grid_map_get_cell(&map, 10, 20);
    CHECK(v == 255, "set/get cell should round-trip");

    int gx, gy;
    int rc = mr_grid_map_world_to_grid(&map, 2.0, 3.0, &gx, &gy);
    CHECK(rc == 0, "world_to_grid should succeed");

    double wx, wy;
    mr_grid_map_grid_to_world(&map, gx, gy, &wx, &wy);
    CHECK(fabs(wx - 2.0) < 0.1, "grid_to_world should be near original");

    mr_point2d a = { 0, 0 }, b = { 10, 10 };
    double h = mr_heuristic_euclidean(&a, &b);
    CHECK(h > 0.0, "euclidean heuristic should be positive");
    double m = mr_heuristic_manhattan(&a, &b);
    CHECK(m > 0.0, "manhattan heuristic should be positive");
    PASS();
    return 0;
}

static int test_a_star_rrt_trajectory(void) {
    TEST("mr_a_star / mr_rrt / mr_trajectory_from_waypoints / mr_dijkstra");
    mr_grid_map map;
    mr_grid_map_init(&map, 50, 50, 0.1, 0.0, 0.0);
    mr_point2d start = { 1, 1 }, goal = { 48, 48 };

    mr_path2d path;
    int rc = mr_a_star(&map, &start, &goal, &path);
    CHECK(rc == 0 || rc == -1, "A* should return valid codes");

    rc = mr_dijkstra(&map, &start, &goal, &path);
    CHECK(rc == 0 || rc == -1, "Dijkstra should return valid codes");

    mr_world2d world;
    memset(&world, 0, sizeof(world));
    world.width = 5.0; world.height = 5.0;
    mr_config2d c_start = { 0.5, 0.5, 0.0 };
    mr_config2d c_goal = { 4.5, 4.5, 0.0 };
    mr_rrt_tree tree;
    mr_path2d rrt_path;
    mr_rrt_init(&tree, &world, 0.1, 0.05);
    rc = mr_rrt_plan(&tree, &c_start, &c_goal, 500, &rrt_path);
    CHECK(rc == 0, "RRT plan should succeed");

    mr_rrt_smooth_path(&rrt_path, &path);
    mr_trajectory traj;
    mr_trajectory_from_waypoints(&path, 0.5, 0.2, &traj);
    CHECK(traj.num_points > 0, "trajectory should have points");
    PASS();
    return 0;
}

static int test_collision_potential(void) {
    TEST("mr_collision_check_point / mr_repulsive_potential");
    mr_world2d world;
    memset(&world, 0, sizeof(world));
    world.width = 10.0; world.height = 10.0;

    mr_point2d pt = { 5.0, 5.0 };
    int hit = mr_collision_check_point(&pt, &world);
    CHECK(hit >= 0, "collision check should return valid result");

    mr_point2d force;
    double pot = mr_repulsive_potential(&pt, &world, 0.5, 2.0, &force);
    CHECK(pot >= 0.0, "potential should be non-negative");
    PASS();
    return 0;
}

/* ================================================================
 *  ros_core.h
 * ================================================================ */

static int test_master_node_message(void) {
    TEST("mr_master_init / node_init / message serialize round-trip");
    mr_master master;
    mr_master_init(&master);
    int rc = mr_master_register_topic(&master, "/cmd_vel", "geometry_msgs/Twist");
    CHECK(rc >= 0, "register topic should succeed");
    int idx = mr_master_find_topic(&master, "/cmd_vel");
    CHECK(idx >= 0, "should find registered topic");

    mr_node node;
    mr_node_init(&node, "test_node");
    mr_msg_definition def;
    memset(&def, 0, sizeof(def));
    mr_message msg;
    mr_message_init(&msg, &def);
    mr_message_set_float32(&msg, 0, 3.14f);
    float v = mr_message_get_float32(&msg, 0);
    CHECK(fabsf(v - 3.14f) < 0.01f, "float32 should round-trip");

    uint8_t ser_buf[MR_ROS_MAX_MSG_SIZE];
    int ser_len;
    mr_message_serialize(&msg, ser_buf, &ser_len);
    CHECK(ser_len > 0, "serialize should produce data");

    mr_message msg2;
    memset(&msg2, 0, sizeof(msg2));
    rc = mr_message_deserialize(ser_buf, ser_len, &msg2);
    CHECK(rc == 0, "deserialize should succeed");

    rc = mr_master_unregister_topic(&master, "/cmd_vel");
    CHECK(rc >= 0, "unregister should succeed");
    PASS();
    return 0;
}

static int test_bag_tf_launch(void) {
    TEST("mr_bag_writer / mr_tf_tree / mr_launch_file");
    mr_bag_writer bag;
    memset(&bag, 0, sizeof(bag));
    mr_bag_writer_open(&bag, "test.bag");
    mr_bag_writer_close(&bag);

    mr_tf_tree tf;
    mr_tf_tree_init(&tf);
    double trans[3] = { 0.1, 0.0, 0.0 };
    double rot[4] = { 0.0, 0.0, 0.0, 1.0 };
    int rc = mr_tf_tree_add_transform(&tf, "base_link", "laser", trans, rot);
    CHECK(rc >= 0, "add_transform should succeed");

    double t_out[3], r_out[4];
    rc = mr_tf_tree_lookup(&tf, "base_link", "laser", t_out, r_out);
    CHECK(rc == 0, "lookup should find transform");

    mr_launch_file lf;
    memset(&lf, 0, sizeof(lf));
    mr_launch_file_parse("<launch></launch>", &lf);
    PASS();
    return 0;
}

/* ================================================================
 *  slam_system.h
 * ================================================================ */

static int test_ekf_slam(void) {
    TEST("mr_ekf_slam_init / predict / update / add/get landmark");
    mr_ekf_slam slam;
    mr_ekf_slam_init(&slam);
    mr_ekf_slam_predict(&slam, 0.1, 0.0, 0.01);
    mr_landmark_obs obs[2];
    obs[0].range = 2.0; obs[0].bearing = 0.5; obs[0].id = 1;
    obs[1].range = 3.0; obs[1].bearing = -0.3; obs[1].id = 2;
    int rc = mr_ekf_slam_update(&slam, obs, 2);
    CHECK(rc >= 0, "EKF update should succeed");

    mr_robot_pose pose;
    mr_ekf_slam_get_pose(&slam, &pose);
    CHECK(isfinite(pose.x) && isfinite(pose.y) && isfinite(pose.theta), "pose should be finite");

    rc = mr_ekf_slam_add_landmark(&slam, 3, 4.0, 0.8);
    CHECK(rc >= 0, "add_landmark should succeed");

    double lx, ly;
    rc = mr_ekf_slam_get_landmark(&slam, 3, &lx, &ly);
    CHECK(rc >= 0, "get_landmark should succeed");
    PASS();
    return 0;
}

static int test_fast_slam(void) {
    TEST("mr_fast_slam_init / predict / update / resample / best_pose");
    mr_fast_slam fs;
    mr_fast_slam_init(&fs, 100);
    CHECK(fs.num_particles == 100, "should have 100 particles");

    mr_fast_slam_predict(&fs, 0.05, 0.0, 0.005, 0.1);
    mr_landmark_obs obs[1];
    obs[0].range = 2.0; obs[0].bearing = 0.3; obs[0].id = 1;
    mr_fast_slam_update(&fs, obs, 1);
    mr_fast_slam_resample(&fs);

    mr_robot_pose pose;
    mr_fast_slam_best_pose(&fs, &pose);
    CHECK(isfinite(pose.x) && isfinite(pose.y) && isfinite(pose.theta), "best pose should be finite");
    PASS();
    return 0;
}

static int test_graph_slam(void) {
    TEST("mr_graph_slam_init + add_node/edge + optimize + loop_closure");
    mr_graph_slam gs;
    mr_robot_pose initial = { 0, 0, 0 };
    mr_graph_slam_init(&gs, &initial);
    CHECK(gs.num_nodes >= 0, "graph should be initialized");

    int n0 = mr_graph_slam_add_node(&gs, &initial);
    CHECK(n0 == 0, "first node should be index 0");

    mr_robot_pose p2 = { 0.1, 0.0, 0.01 };
    int n1 = mr_graph_slam_add_node(&gs, &p2);
    CHECK(n1 == 1, "second node should be index 1");

    int rc = mr_graph_slam_add_edge(&gs, 0, 1, 0.1, 0.0, 0.01);
    CHECK(rc >= 0, "add_edge should succeed");

    rc = mr_graph_slam_optimize(&gs, 5);
    CHECK(rc >= 0, "optimize should succeed");

    rc = mr_graph_slam_loop_closure(&gs, 0, 1, 0.1, 0.0, 0.01);
    CHECK(rc >= 0, "loop_closure should succeed");
    PASS();
    return 0;
}

static int test_occ_grid(void) {
    TEST("mr_occ_grid_init + update_from_scan + get + occupied");
    mr_occ_grid grid;
    mr_occ_grid_init(&grid, 100, 100, 0.1, 0.0, 0.0);
    CHECK(grid.width == 100 && grid.height == 100, "grid should be 100x100");

    mr_robot_pose pose = { 5.0, 5.0, 0.0 };
    double ranges[180];
    for (int k = 0; k < 180; k++) ranges[k] = 3.0 + (k % 5) * 0.1;
    mr_occ_grid_update_from_scan(&grid, &pose, ranges, 180,
        -M_PI_2, M_PI / 180.0, 10.0);

    int valid;
    uint8_t v = mr_occ_grid_get(&grid, 50, 50, &valid);
    (void)v;
    double prob = mr_occ_grid_occupied(&grid, 4.5, 5.0);
    CHECK(prob >= 0.0 && prob <= 1.0, "occupancy probability should be in [0,1]");
    PASS();
    return 0;
}

/* ================================================================
 *  main
 * ================================================================ */

int main(void) {
    printf("mini-robotics  --  Core Unit Tests\n\n");

    /* kinematics.h */
    test_dh_transform();
    test_mat4_identity_inverse();
    test_mat4_multiply();
    test_pose_mat4_conversion();
    test_forward_kinematics();
    test_inverse_kinematics();
    test_jacobian_manipulability();

    /* mobile_robot.h */
    test_diff_drive_odometry();
    test_imu_init_update();
    test_laser_scan();
    test_amcl();
    test_pid_navigation();

    /* path_planning.h */
    test_grid_map_heuristic();
    test_a_star_rrt_trajectory();
    test_collision_potential();

    /* ros_core.h */
    test_master_node_message();
    test_bag_tf_launch();

    /* slam_system.h */
    test_ekf_slam();
    test_fast_slam();
    test_graph_slam();
    test_occ_grid();

    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
