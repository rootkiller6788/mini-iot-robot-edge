#include "slam_system.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void print_pose(const char *label, const mr_robot_pose *p)
{
    printf("%s: (%.3f, %.3f, %.3f rad)\n", label, p->x, p->y, p->theta);
}

int main(void)
{
    srand(42);

    printf("=== SLAM Demo ===\n\n");

    printf("--- EKF-SLAM ---\n\n");

    mr_ekf_slam ekf;
    mr_ekf_slam_init(&ekf);

    print_pose("Initial pose", &ekf.pose);

    double dt = 0.1;
    int step;
    for (step = 0; step < 5; step++) {
        mr_ekf_slam_predict(&ekf, 0.5, 0.0, 0.0);
        printf("Step %d after predict: ", step + 1);
        print_pose("", &ekf.pose);
    }

    mr_landmark_obs obs[2];
    obs[0].range = 3.0;
    obs[0].bearing = 0.3;
    obs[0].id = 0;

    obs[1].range = 2.5;
    obs[1].bearing = -0.5;
    obs[1].id = 1;

    printf("\nObservations:\n");
    printf("  LM0: range=3.0, bearing=0.3 rad\n");
    printf("  LM1: range=2.5, bearing=-0.5 rad\n");

    mr_ekf_slam_update(&ekf, obs, 2);

    double lx, ly;
    int i;
    for (i = 0; i < 2; i++) {
        if (mr_ekf_slam_get_landmark(&ekf, i, &lx, &ly))
            printf("  Landmark %d: (%.3f, %.3f)\n", i, lx, ly);
    }

    print_pose("After EKF update", &ekf.pose);
    printf("Num landmarks: %d\n", ekf.num_landmarks);

    for (step = 0; step < 3; step++) {
        mr_ekf_slam_predict(&ekf, 0.0, 0.3, 0.15);
        printf("  Step %d: ", step + 6);
        print_pose("", &ekf.pose);
    }

    printf("\n--- FastSLAM (Particle Filter) ---\n\n");

    mr_fast_slam fs;
    mr_fast_slam_init(&fs, 128);

    printf("Initialized %d particles\n", fs.num_particles);

    for (step = 0; step < 5; step++) {
        mr_fast_slam_predict(&fs, 0.4, 0.0, 0.0, 0.1);
        mr_robot_pose best;
        mr_fast_slam_best_pose(&fs, &best);
        printf("  Step %d best: ", step + 1);
        print_pose("", &best);
    }

    mr_landmark_obs fsobs[3];
    fsobs[0].id = 0; fsobs[0].range = 4.0; fsobs[0].bearing = 0.1;
    fsobs[1].id = 1; fsobs[1].range = 3.0; fsobs[1].bearing = -0.3;
    fsobs[2].id = -1; fsobs[2].range = 5.0; fsobs[2].bearing = 0.5;

    mr_fast_slam_update(&fs, fsobs, 3);

    printf("After observation update:\n");
    for (i = 0; i < 3 && i < fs.num_particles; i++)
        printf("  Particle %d weight: %.6f\n",
               i, fs.particles[i].weight);

    mr_fast_slam_resample(&fs);
    printf("After resampling:\n");
    for (i = 0; i < 3 && i < fs.num_particles; i++)
        printf("  Particle %d weight: %.4f\n",
               i, fs.particles[i].weight);

    mr_robot_pose final_pose;
    mr_fast_slam_best_pose(&fs, &final_pose);
    print_pose("Final best pose", &final_pose);

    printf("\n--- Graph SLAM ---\n\n");

    mr_graph_slam gs;
    mr_robot_pose origin = {0.0, 0.0, 0.0};
    mr_graph_slam_init(&gs, &origin);

    printf("Node 0: (0.0, 0.0, 0.0)\n");

    mr_robot_pose nodes[5] = {
        {1.0, 0.1, 0.05},
        {2.0, 0.3, 0.1},
        {3.0, 0.2, 0.08},
        {4.0, 0.4, 0.15},
        {4.8, -0.1, 0.02}
    };

    for (i = 0; i < 5; i++) {
        int idx = mr_graph_slam_add_node(&gs, &nodes[i]);
        mr_graph_slam_add_edge(&gs, i, i + 1,
                               nodes[i].x - (i == 0 ? 0.0 : nodes[i - 1].x),
                               nodes[i].y - (i == 0 ? 0.0 : nodes[i - 1].y),
                               nodes[i].theta - (i == 0 ? 0.0 : nodes[i - 1].theta));
        printf("Node %d: (%.1f, %.1f, %.2f)\n",
               idx, nodes[i].x, nodes[i].y, nodes[i].theta);
    }

    mr_graph_slam_add_edge(&gs, 0, 5, 5.0, 0.05, 0.0);
    printf("Loop closure edge: node 0 -> node 5\n");

    mr_graph_slam_optimize(&gs, 50);

    printf("\nAfter optimization:\n");
    for (i = 0; i < gs.num_nodes; i++)
        printf("  Node %d: (%.3f, %.3f, %.3f)\n",
               i, gs.nodes[i].x, gs.nodes[i].y, gs.nodes[i].theta);

    printf("\n--- Occupancy Grid Mapping ---\n\n");

    mr_occ_grid grid;
    mr_occ_grid_init(&grid, 64, 64, 0.1, -3.2, -3.2);

    mr_robot_pose scan_pose = {0.5, 0.5, 0.0};
    double ranges[360];
    double a_min = -M_PI_2, a_inc = M_PI / 180.0;

    for (i = 0; i < 360; i++) {
        double angle = a_min + (double)i * a_inc + 0.01 * (double)(rand() % 100) / 99.0;
        ranges[i] = 2.0 + 0.5 * sin(3.0 * angle) + 0.1 * (double)(rand() % 100) / 99.0;
    }

    mr_occ_grid_update_from_scan(&grid, &scan_pose, ranges, 360,
                                 a_min, a_inc, 10.0);

    printf("Grid: %dx%d, resolution=%.2f\n",
           grid.width, grid.height, grid.resolution);

    int occupied_count = 0, free_count = 0, unknown_count = 0;
    int x, y;
    for (y = 0; y < grid.height; y++)
        for (x = 0; x < grid.width; x++) {
            uint8_t val = grid.cells[y * grid.width + x];
            if (val > 200) occupied_count++;
            else if (val < 50) free_count++;
            else unknown_count++;
        }
    printf("  Occupied: %d, Free: %d, Unknown: %d\n",
           occupied_count, free_count, unknown_count);

    double occ = mr_occ_grid_occupied(&grid, 1.0, 1.0);
    printf("  Occupancy at (1.0, 1.0): %.3f\n", occ);

    printf("\n--- Loop Closure Descriptor Matching ---\n\n");

    double scan1[16] = {1.0, 2.0, 3.0, 2.5, 1.5, 0.8, 0.5, 1.0,
                        1.5, 3.0, 4.0, 3.5, 2.0, 1.0, 0.6, 0.4};
    double scan2[16] = {1.1, 2.1, 3.1, 2.6, 1.6, 0.9, 0.6, 1.1,
                        1.6, 3.1, 4.1, 3.6, 2.1, 1.1, 0.7, 0.5};
    double scan3[16] = {5.0, 1.0, 0.5, 4.0, 2.0, 0.7, 3.0, 1.5,
                        0.8, 2.5, 4.5, 1.2, 3.5, 0.9, 2.2, 5.5};

    mr_descriptor d1 = mr_compute_scan_descriptor(scan1, 16);
    mr_descriptor d2 = mr_compute_scan_descriptor(scan2, 16);
    mr_descriptor d3 = mr_compute_scan_descriptor(scan3, 16);

    printf("Similarity(scan1, scan2) = %.4f (should be high)\n",
           mr_descriptor_similarity(&d1, &d2));
    printf("Similarity(scan1, scan3) = %.4f (should be low)\n",
           mr_descriptor_similarity(&d1, &d3));

    mr_descriptor_free(&d1);
    mr_descriptor_free(&d2);
    mr_descriptor_free(&d3);

    printf("\nDone.\n");
    return 0;
}
