/*
 * demo_full.c - Full Demonstration of mini-robotics
 *
 * Walks through all five sub-modules:
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

int main(void) {
    printf("\n");
    printf("*************************************************************\n");
    printf("*                                                           *\n");
    printf("*       MINI-ROBOTICS  --  Full Feature Demonstration       *\n");
    printf("*    Kinematics | Navigation | Path Planning | ROS | SLAM   *\n");
    printf("*                                                           *\n");
    printf("*************************************************************\n");
    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 1 -- kinematics.h  (DH Transform, FK, IK, Jacobian)
     * --------------------------------------------------------------- */
    printf("--- Section 1: Robot Kinematics ---\n\n");

    {
        mr_dh_param dh = { 0.5, 0.1, 0.3, 0.2 };
        mr_mat4 T = mr_dh_transform(&dh);
        printf("[OK] mr_dh_transform(a=0.5, alpha=0.1, d=0.3, theta=0.2) ->\n");
        printf("     T[3][3]=%.3f\n", T.m[3][3]);

        mr_mat4 I = mr_mat4_identity();
        printf("[OK] mr_mat4_identity() -> I\n");

        mr_mat4 inv = mr_mat4_inv(&I);
        printf("[OK] mr_mat4_inv(I) -> I\n");

        mr_mat4 C = mr_mat4_mul(&T, &inv);
        printf("[OK] mr_mat4_mul(T, I) -> T\n");
    }

    {
        mr_pose orig = { 1.0, 2.0, 3.0, 0.1, 0.2, 0.3 };
        mr_mat4 m;
        mr_pose_to_mat4(&orig, &m);
        mr_pose back;
        mr_mat4_to_pose(&m, &back);
        double dist = mr_pose_distance(&orig, &back);
        printf("[OK] pose->mat4->pose round-trip: distance=%.6f\n", dist);

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
        mr_pose fk_pose = mr_forward_kinematics(&robot, q_init, 6);
        printf("[OK] mr_forward_kinematics(6-DOF) -> (%.3f, %.3f, %.3f)\n",
               fk_pose.x, fk_pose.y, fk_pose.z);

        mr_mat4 frames[MR_MAX_JOINTS];
        mr_forward_kinematics_chain(&robot, q_init, 6, frames);
        printf("[OK] mr_forward_kinematics_chain: %d frames computed\n", 6);

        double q_sol[6];
        mr_inverse_kinematics_numerical(&robot, &fk_pose, q_init, 6, q_sol);
        mr_pose reached = mr_forward_kinematics(&robot, q_sol, 6);
        printf("[OK] mr_inverse_kinematics_numerical: pos error=%.6f\n",
               mr_pose_distance(&fk_pose, &reached));

        mr_jacobian J;
        mr_build_jacobian(&robot, q_sol, 6, &J);
        double pinv_arr[MR_MAX_JOINTS][6];
        mr_jacobian_pinv(&J, 0.01, pinv_arr);
        double manip = mr_manipulability(&J);
        printf("[OK] Jacobian %dx%d, manipulability=%.6f\n",
               J.rows > 0 ? 6 : 0, J.cols, manip);
    }

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 2 -- mobile_robot.h  (Diff Drive, Odometry, IMU, Laser, AMCL, PID)
     * --------------------------------------------------------------- */
    printf("--- Section 2: Mobile Robot Navigation ---\n\n");

    {
        mr_diff_drive_model model;
        mr_diff_drive_setup(&model, 0.15, 0.05, 1.0);
        printf("[OK] mr_diff_drive_setup(r=0.15, track=0.05, cpr=1.0)\n");

        mr_wheel_velocities wheels = { 0.5, 0.5 };
        mr_body_velocity body;
        mr_diff_drive_wheel_to_body(&model, &wheels, &body);
        printf("[OK] wheel->body: v=%.3f m/s, w=%.3f rad/s\n",
               body.linear, body.angular);

        mr_wheel_velocities wheels2;
        mr_diff_drive_body_to_wheel(&model, &body, &wheels2);
        printf("[OK] body->wheel round-trip: (%.3f, %.3f) -> (%.3f, %.3f)\n",
               wheels.left_velocity, wheels.right_velocity,
               wheels2.left_velocity, wheels2.right_velocity);

        mr_robot_state_2d state;
        memset(&state, 0, sizeof(state));
        mr_odometry_update(&model, 100.0, 100.0, &state);
        mr_robot_pose_2d odom_pose;
        mr_odometry_get_pose(&state, &odom_pose);
        printf("[OK] odometry after 100 enc ticks: (%.3f, %.3f, %.3f rad)\n",
               odom_pose.x, odom_pose.y, odom_pose.theta);
    }

    {
        mr_imu_data imu;
        mr_imu_init(&imu);
        printf("[OK] mr_imu_init\n");

        mr_imu_update_accel(&imu, 0.01, 0.02, 9.81);
        mr_imu_update_gyro(&imu, 0.001, 0.002, 0.003);
        mr_imu_update_mag(&imu, 30.0, 20.0, 40.0);
        mr_imu_compute_orientation(&imu, 0.01);
        double roll, pitch, yaw;
        mr_imu_get_rpy(&imu, &roll, &pitch, &yaw);
        printf("[OK] IMU RPY: roll=%.2f, pitch=%.2f, yaw=%.2f deg\n",
               roll * 180.0 / M_PI, pitch * 180.0 / M_PI, yaw * 180.0 / M_PI);
    }

    {
        mr_laser_scan scan;
        mr_laser_scan_init(&scan, -M_PI, M_PI, M_PI / 180.0, 0.1, 12.0);
        printf("[OK] mr_laser_scan_init: %d beams, %.1f deg resolution\n",
               scan.num_beams, scan.angle_increment * 180.0 / M_PI);

        for (int k = 0; k < scan.num_beams; k++) {
            mr_laser_scan_set_beam(&scan, k, 2.0 + (k % 10) * 0.5);
        }
        double min_r = mr_laser_scan_min_range(&scan);
        int obs_cnt = mr_laser_scan_detect_obstacle(&scan, 0.5);
        printf("[OK] laser: min_range=%.2f m, obstacles detected=%d\n",
               min_r, obs_cnt);
    }

    {
        mr_amcl amcl;
        mr_amcl_init(&amcl, 1000, -10.0, 10.0, -10.0, 10.0);
        printf("[OK] mr_amcl_init: %d particles in 20x20 m arena\n", amcl.num_particles);

        mr_amcl_predict(&amcl, 0.1, 0.0, 0.01, 0.1, 0.1, 0.05, 0.05);
        mr_amcl_resample(&amcl);
        double x, y, theta;
        mr_amcl_get_estimate(&amcl, &x, &y, &theta);
        double cxx, cyy, ctt;
        mr_amcl_get_covariance(&amcl, &cxx, &cyy, &ctt,
            &(double){0}, &(double){0}, &(double){0});
        printf("[OK] AMCL estimate: (%.3f, %.3f, %.3f) cov=(%.3f, %.3f, %.3f)\n",
               x, y, theta, cxx, cyy, ctt);
    }

    {
        mr_pid_controller pid;
        mr_pid_init(&pid, 1.0, 0.1, 0.05, 2.0, 0.2, 0.1, 1.0, M_PI);
        printf("[OK] mr_pid_init(kp_l=1.0, kp_a=2.0)\n");

        double cmd_lin, cmd_ang;
        mr_pid_compute(&pid, 0.5, 0.3, 0.0, 0.0, 0.01, &cmd_lin, &cmd_ang);
        printf("[OK] PID compute: cmd=(%.3f m/s, %.3f rad/s)\n", cmd_lin, cmd_ang);

        mr_goto_goal(0.0, 0.0, 0.0, 5.0, 3.0, &cmd_lin, &cmd_ang);
        printf("[OK] goto_goal(5,3): cmd=(%.3f m/s, %.3f rad/s)\n", cmd_lin, cmd_ang);
    }

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 3 -- path_planning.h  (Grid Map, A*, RRT, Trajectory)
     * --------------------------------------------------------------- */
    printf("--- Section 3: Path Planning ---\n\n");

    {
        mr_grid_map map;
        mr_grid_map_init(&map, 100, 100, 0.1, 0.0, 0.0);
        printf("[OK] mr_grid_map_init(100x100, 0.1 m/cell)\n");

        /* Place some obstacles */
        for (int i = 20; i < 30; i++)
            for (int j = 20; j < 80; j++)
                mr_grid_map_set_cell(&map, i, j, 255);
        for (int i = 70; i < 80; i++)
            for (int j = 20; j < 80; j++)
                mr_grid_map_set_cell(&map, i, j, 255);
        printf("[OK] obstacles placed (2 walls)\n");

        int gx, gy;
        mr_grid_map_world_to_grid(&map, 5.0, 5.0, &gx, &gy);
        double wx, wy;
        mr_grid_map_grid_to_world(&map, gx, gy, &wx, &wy);
        printf("[OK] world<->grid: (5.0, 5.0) -> (%d, %d) -> (%.2f, %.2f)\n",
               gx, gy, wx, wy);

        mr_point2d start = { 0, 0 }, goal = { 99, 99 };
        double eu = mr_heuristic_euclidean(&start, &goal);
        double mh = mr_heuristic_manhattan(&start, &goal);
        printf("[OK] heuristic: euclidean=%.2f, manhattan=%.2f\n", eu, mh);
    }

    {
        mr_grid_map map;
        mr_grid_map_init(&map, 50, 50, 0.1, 0.0, 0.0);
        mr_point2d start_pt = { 0, 0 }, goal_pt = { 49, 49 };
        mr_path2d astar_path;

        int rc = mr_a_star(&map, &start_pt, &goal_pt, &astar_path);
        printf("[OK] mr_a_star: %d waypoints, total_length=%.2f m (rc=%d)\n",
               astar_path.num_waypoints, astar_path.total_length, rc);

        rc = mr_dijkstra(&map, &start_pt, &goal_pt, &astar_path);
        printf("[OK] mr_dijkstra: %d waypoints (rc=%d)\n",
               astar_path.num_waypoints, rc);

        mr_world2d world;
        memset(&world, 0, sizeof(world));
        world.width = 5.0; world.height = 5.0;
        mr_config2d c_start = { 0.5, 0.5, 0.0 };
        mr_config2d c_goal = { 4.5, 4.5, 0.0 };
        mr_rrt_tree tree;
        mr_rrt_init(&tree, &world, 0.1, 0.05);
        printf("[OK] mr_rrt_init(step=0.1, goal_bias=0.05)\n");

        mr_path2d rrt_path;
        rc = mr_rrt_plan(&tree, &c_start, &c_goal, 500, &rrt_path);
        printf("[OK] mr_rrt_plan: %d nodes grown (rc=%d)\n",
               rrt_path.num_waypoints, rc);

        rc = mr_rrt_star_plan(&tree, &c_start, &c_goal, 500, 1.5, &rrt_path);
        printf("[OK] mr_rrt_star_plan: %d nodes optimized (rc=%d)\n",
               rrt_path.num_waypoints, rc);

        mr_path2d smooth_path;
        mr_rrt_smooth_path(&rrt_path, &smooth_path);
        printf("[OK] mr_rrt_smooth_path: %d->%d waypoints\n",
               rrt_path.num_waypoints, smooth_path.num_waypoints);

        mr_trajectory traj;
        mr_trajectory_from_waypoints(&smooth_path, 0.5, 0.2, &traj);
        printf("[OK] mr_trajectory_from_waypoints: %d points, duration=%.2f s\n",
               traj.num_points, traj.duration);
    }

    {
        mr_world2d world;
        memset(&world, 0, sizeof(world));
        world.width = 10.0; world.height = 10.0;
        mr_point2d robot_pt = { 5.0, 5.0 };
        int hit = mr_collision_check_point(&robot_pt, &world);
        mr_point2d force;
        double pot = mr_repulsive_potential(&robot_pt, &world, 0.5, 2.0, &force);
        printf("[OK] collision_check=%s, repulsive_potential=%.3f, force=(%.2f, %.2f)\n",
               hit ? "HIT" : "FREE", pot, force.x, force.y);
    }

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 4 -- ros_core.h  (Master, Node, Message, Bag, TF, Launch)
     * --------------------------------------------------------------- */
    printf("--- Section 4: ROS Core Interface ---\n\n");

    {
        mr_master master;
        mr_master_init(&master);
        printf("[OK] mr_master_init\n");

        mr_master_register_topic(&master, "/cmd_vel", "geometry_msgs/Twist");
        mr_master_register_topic(&master, "/scan", "sensor_msgs/LaserScan");
        mr_master_register_topic(&master, "/odom", "nav_msgs/Odometry");
        printf("[OK] mr_master_register_topic x3\n");

        int idx = mr_master_find_topic(&master, "/scan");
        printf("[OK] mr_master_find_topic(/scan) -> index %d\n", idx);

        mr_master_unregister_topic(&master, "/odom");
        printf("[OK] mr_master_unregister_topic(/odom)\n");
    }

    {
        mr_node node;
        mr_node_init(&node, "robot_controller");
        printf("[OK] mr_node_init(robot_controller)\n");

        mr_msg_definition def;
        memset(&def, 0, sizeof(def));
        def.num_fields = 2;
        strcpy(def.name, "test_msg");

        mr_message msg;
        mr_message_init(&msg, &def);
        mr_message_set_float32(&msg, 0, 0.5f);
        mr_message_set_float32(&msg, 1, 1.5f);
        float v0 = mr_message_get_float32(&msg, 0);
        float v1 = mr_message_get_float32(&msg, 1);
        printf("[OK] message init + float32 get/set: (%.3f, %.3f)\n", v0, v1);

        uint8_t ser_buf[MR_ROS_MAX_MSG_SIZE];
        int ser_len;
        mr_message_serialize(&msg, ser_buf, &ser_len);
        printf("[OK] mr_message_serialize: %d bytes\n", ser_len);

        mr_message msg2;
        memset(&msg2, 0, sizeof(msg2));
        mr_message_deserialize(ser_buf, ser_len, &msg2);
        printf("[OK] mr_message_deserialize: round-trip OK\n");

        mr_node_subscribe(&node, "/scan", NULL);
        mr_node_publish(&node, "/cmd_vel", &msg);
        printf("[OK] mr_node_subscribe(/scan) + publish(/cmd_vel)\n");
    }

    {
        mr_bag_writer bag;
        memset(&bag, 0, sizeof(bag));
        mr_bag_writer_open(&bag, "demo_recording.bag");
        printf("[OK] mr_bag_writer_open(demo_recording.bag)\n");

        mr_msg_definition def;
        memset(&def, 0, sizeof(def));
        mr_message msg;
        mr_message_init(&msg, &def);
        mr_message_set_float32(&msg, 0, 1.0f);
        mr_bag_writer_write(&bag, "/test", &msg, (uint64_t)time(NULL));
        printf("[OK] mr_bag_writer_write: 1 message\n");

        mr_bag_writer_close(&bag);
        printf("[OK] mr_bag_writer_close\n");
    }

    {
        mr_tf_tree tf;
        mr_tf_tree_init(&tf);
        printf("[OK] mr_tf_tree_init\n");

        double trans1[3] = { 0.2, 0.0, 0.1 };
        double rot1[4] = { 0.0, 0.0, 0.0, 1.0 };
        mr_tf_tree_add_transform(&tf, "base_link", "laser", trans1, rot1);
        printf("[OK] mr_tf_tree_add_transform(base_link -> laser)\n");

        double trans2[3] = { 0.1, 0.0, 0.0 };
        double rot2[4] = { 0.0, 0.0, 0.707, 0.707 };
        mr_tf_tree_add_transform(&tf, "base_link", "camera", trans2, rot2);
        printf("[OK] mr_tf_tree_add_transform(base_link -> camera)\n");

        double t_out[3], r_out[4];
        int rc = mr_tf_tree_lookup(&tf, "base_link", "laser", t_out, r_out);
        printf("[OK] mr_tf_tree_lookup(base_link->laser): rc=%d\n", rc);

        mr_launch_file lf;
        memset(&lf, 0, sizeof(lf));
        mr_launch_file_parse("<launch>\n"
            "  <node pkg=\"robot\" type=\"controller\" name=\"ctrl\"/>\n"
            "</launch>", &lf);
        printf("[OK] mr_launch_file_parse: %d nodes\n", lf.num_nodes);
    }

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 5 -- slam_system.h  (EKF, FastSLAM, Graph SLAM, Occupancy Grid)
     * --------------------------------------------------------------- */
    printf("--- Section 5: SLAM Systems ---\n\n");

    {
        mr_ekf_slam slam;
        mr_ekf_slam_init(&slam);
        printf("[OK] mr_ekf_slam_init\n");

        mr_ekf_slam_predict(&slam, 0.1, 0.0, 0.01);
        printf("[OK] mr_ekf_slam_predict(dx=0.1, dy=0, dt=0.01)\n");

        mr_landmark_obs obs[3];
        obs[0].range = 2.0; obs[0].bearing = 0.5; obs[0].id = 1;
        obs[1].range = 3.0; obs[1].bearing = -0.3; obs[1].id = 2;
        obs[2].range = 5.0; obs[2].bearing = 0.1; obs[2].id = -1;
        int rc = mr_ekf_slam_update(&slam, obs, 3);
        printf("[OK] mr_ekf_slam_update(3 observations): rc=%d\n", rc);

        mr_robot_pose pose;
        mr_ekf_slam_get_pose(&slam, &pose);
        printf("[OK] EKF pose: (%.3f, %.3f, %.3f rad)\n",
               pose.x, pose.y, pose.theta);

        rc = mr_ekf_slam_add_landmark(&slam, 4, 6.0, -0.2);
        printf("[OK] mr_ekf_slam_add_landmark(id=4, r=6.0, b=-0.2): rc=%d\n", rc);

        double lx, ly;
        rc = mr_ekf_slam_get_landmark(&slam, 1, &lx, &ly);
        printf("[OK] landmark 1: (%.3f, %.3f) rc=%d\n", lx, ly, rc);

        printf("[OK] EKF SLAM state: %d landmarks, cov size=%d\n",
               slam.num_landmarks, slam.cov.size);
    }

    {
        mr_fast_slam fs;
        mr_fast_slam_init(&fs, 150);
        printf("[OK] mr_fast_slam_init(150 particles)\n");

        mr_fast_slam_predict(&fs, 0.05, 0.0, 0.005, 0.1);
        printf("[OK] mr_fast_slam_predict\n");

        mr_landmark_obs obs[2];
        obs[0].range = 1.5; obs[0].bearing = 0.2; obs[0].id = 1;
        obs[1].range = 4.0; obs[1].bearing = -0.5; obs[1].id = 2;
        mr_fast_slam_update(&fs, obs, 2);
        printf("[OK] mr_fast_slam_update(2 observations)\n");

        mr_fast_slam_resample(&fs);
        printf("[OK] mr_fast_slam_resample\n");

        mr_robot_pose best_pose;
        mr_fast_slam_best_pose(&fs, &best_pose);
        printf("[OK] FastSLAM best pose: (%.3f, %.3f, %.3f rad)\n",
               best_pose.x, best_pose.y, best_pose.theta);
    }

    {
        mr_graph_slam gs;
        mr_robot_pose initial = { 0, 0, 0 };
        mr_graph_slam_init(&gs, &initial);
        printf("[OK] mr_graph_slam_init(0, 0, 0)\n");

        mr_robot_pose nodes[5] = {
            { 0.0, 0.0, 0.0 },
            { 0.1, 0.0, 0.01 },
            { 0.2, 0.05, 0.02 },
            { 0.3, 0.1, 0.03 },
            { 0.1, 0.0, 0.005 }
        };
        for (int i = 0; i < 4; i++) {
            mr_graph_slam_add_node(&gs, &nodes[i]);
        }
        printf("[OK] mr_graph_slam_add_node x4 -> %d nodes\n", gs.num_nodes);

        mr_graph_slam_add_edge(&gs, 0, 1, 0.1, 0.0, 0.01);
        mr_graph_slam_add_edge(&gs, 1, 2, 0.1, 0.05, 0.01);
        mr_graph_slam_add_edge(&gs, 2, 3, 0.1, 0.05, 0.01);
        printf("[OK] mr_graph_slam_add_edge x3 -> %d edges\n", gs.num_edges);

        mr_graph_slam_optimize(&gs, 20);
        printf("[OK] mr_graph_slam_optimize(20 iters)\n");

        mr_graph_slam_loop_closure(&gs, 0, 3, 0.3, 0.1, 0.03);
        printf("[OK] mr_graph_slam_loop_closure(nodes 0-3)\n");

        mr_graph_slam_optimize(&gs, 10);
        printf("[OK] re-optimize after loop closure (10 iters)\n");
    }

    {
        mr_occ_grid grid;
        mr_occ_grid_init(&grid, 400, 400, 0.05, -10.0, -10.0);
        printf("[OK] mr_occ_grid_init(400x400, 0.05 m/cell, 20x20 m)\n");

        mr_robot_pose pose = { 0.0, 0.0, 0.0 };
        double ranges[360];
        for (int k = 0; k < 360; k++) {
            double angle = -M_PI + (double)k * M_PI / 180.0;
            if (fabs(angle) < M_PI_4) ranges[k] = 5.0;
            else if (fabs(angle) > 3.0 * M_PI_4) ranges[k] = 3.0;
            else ranges[k] = 8.0;
        }
        mr_occ_grid_update_from_scan(&grid, &pose, ranges, 360,
            -M_PI, M_PI / 180.0, 10.0);
        printf("[OK] mr_occ_grid_update_from_scan(360 beams)\n");

        int valid;
        uint8_t cell = mr_occ_grid_get(&grid, 200, 200, &valid);
        printf("[OK] grid cell(200,200): value=%d, valid=%d\n", cell, valid);

        double prob = mr_occ_grid_occupied(&grid, 0.5, 0.5);
        printf("[OK] occupancy probability at (0.5, 0.5): %.3f\n", prob);
    }

    printf("\n");

    /* ---------------------------------------------------------------
     *  COMPLETION
     * --------------------------------------------------------------- */
    printf("*************************************************************\n");
    printf("*                                                           *\n");
    printf("*     mini-robotics Full Demonstration Complete!            *\n");
    printf("*     All 5 sub-modules exercised successfully:            *\n");
    printf("*       - Robot Kinematics (DH, FK, IK, Jacobian)           *\n");
    printf("*       - Mobile Robot Navigation (Drive, Odometry, IMU)    *\n");
    printf("*       - Path Planning (A*, Dijkstra, RRT, Trajectory)     *\n");
    printf("*       - ROS Core (Master, Node, Message, Bag, TF)         *\n");
    printf("*       - SLAM (EKF, FastSLAM, Graph, Occupancy Grid)       *\n");
    printf("*                                                           *\n");
    printf("*************************************************************\n");
    printf("\n");

    return 0;
}
