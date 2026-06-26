#include "path_planning.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

static void print_grid(const mr_grid_map *map, int *px, int *py, int plen)
{
    int x, y;
    for (y = map->height - 1; y >= 0; y--) {
        for (x = 0; x < map->width; x++) {
            int on_path = 0;
            int k;
            for (k = 0; k < plen; k++)
                if (px[k] == x && py[k] == y) { on_path = 1; break; }

            if (on_path) {
                printf("o");
            } else {
                uint8_t v = mr_grid_map_get_cell(map, x, y);
                printf("%c", v > 200 ? '#' : '.');
            }
        }
        printf("\n");
    }
}

int main(void)
{
    srand(12345);

    int w = 40, h = 30;
    mr_grid_map map;
    map.width = w;
    map.height = h;
    map.resolution = 0.1;
    map.origin_x = -2.0;
    map.origin_y = -1.5;

    printf("=== Path Planning Demo ===\n\n");

    printf("--- A* Grid Search ---\n");

    double start_wx = -1.5, start_wy = -1.0;
    double goal_wx = 1.8, goal_wy = 1.2;

    mr_point2d start = {start_wx, start_wy};
    mr_point2d goal = {goal_wx, goal_wy};

    mr_path2d path;
    int found = mr_a_star(&map, &start, &goal, &path);

    printf("Start: (%.1f, %.1f)  Goal: (%.1f, %.1f)\n",
           start_wx, start_wy, goal_wx, goal_wy);
    printf("Path found: %s\n", found ? "yes" : "no");
    if (found) {
        printf("Waypoints: %d, Length: %.2f m\n",
               path.num_waypoints, path.total_length);
        int i;
        for (i = 0; i < path.num_waypoints; i++)
            printf("  WP[%d]: (%.3f, %.3f)\n",
                   i, path.waypoints[i].x, path.waypoints[i].y);
    }

    printf("\n--- RRT Planning ---\n");

    mr_world2d world;
    world.width = 10.0;
    world.height = 10.0;
    world.num_obstacles = 3;

    world.obstacles[0].type = MR_OBSTACLE_CIRCLE;
    world.obstacles[0].circle.center.x = 5.0;
    world.obstacles[0].circle.center.y = 5.0;
    world.obstacles[0].circle.radius = 1.0;

    world.obstacles[1].type = MR_OBSTACLE_CIRCLE;
    world.obstacles[1].circle.center.x = 3.0;
    world.obstacles[1].circle.center.y = 7.0;
    world.obstacles[1].circle.radius = 0.8;

    world.obstacles[2].type = MR_OBSTACLE_CIRCLE;
    world.obstacles[2].circle.center.x = 7.0;
    world.obstacles[2].circle.center.y = 3.0;
    world.obstacles[2].circle.radius = 0.6;

    mr_rrt_tree tree;
    mr_rrt_init(&tree, &world, 0.5, 0.1);

    mr_config2d rrt_start = {1.0, 1.0, 0.0};
    mr_config2d rrt_goal = {9.0, 9.0, 0.0};

    mr_path2d rrt_path;
    int rrt_found = mr_rrt_plan(&tree, &rrt_start, &rrt_goal,
                                2000, &rrt_path);

    printf("Start: (1.0, 1.0)  Goal: (9.0, 9.0)\n");
    printf("Obstacles: 3 circles\n");
    printf("RRT path found: %s\n", rrt_found ? "yes" : "no");
    if (rrt_found) {
        printf("RRT tree nodes: %d\n", tree.num_nodes);
        printf("Waypoints: %d\n", rrt_path.num_waypoints);
        int i;
        for (i = rrt_path.num_waypoints - 1; i >= 0; i--)
            printf("  WP[%d]: (%.2f, %.2f)\n",
                   rrt_path.num_waypoints - 1 - i,
                   rrt_path.waypoints[i].x, rrt_path.waypoints[i].y);
    }

    printf("\n--- RRT* Planning ---\n");

    mr_rrt_tree tree_star;
    mr_rrt_init(&tree_star, &world, 0.5, 0.1);

    mr_path2d star_path;
    int star_found = mr_rrt_star_plan(&tree_star, &rrt_start, &rrt_goal,
                                      2000, 2.0, &star_path);

    printf("RRT* path found: %s\n", star_found ? "yes" : "no");
    if (star_found) {
        printf("RRT* tree nodes: %d\n", tree_star.num_nodes);
        printf("Waypoints: %d\n", star_path.num_waypoints);

        mr_path2d smooth;
        mr_rrt_smooth_path(&star_path, &smooth);
        printf("After smoothing: %d waypoints\n", smooth.num_waypoints);
    }

    printf("\n--- Trajectory Generation ---\n");

    mr_path2d traj_path;
    traj_path.num_waypoints = 5;
    traj_path.waypoints[0] = (mr_point2d){0.0, 0.0};
    traj_path.waypoints[1] = (mr_point2d){1.0, 0.5};
    traj_path.waypoints[2] = (mr_point2d){2.0, 1.0};
    traj_path.waypoints[3] = (mr_point2d){3.0, 0.5};
    traj_path.waypoints[4] = (mr_point2d){4.0, 0.0};
    traj_path.total_length = 4.2;

    mr_trajectory traj;
    mr_trajectory_from_waypoints(&traj_path, 0.5, 0.2, &traj);
    printf("Trajectory: %d points, duration=%.2f s\n",
           traj.num_points, traj.duration);
    int j;
    for (j = 0; j < traj.num_points && j < 10; j++)
        printf("  t[%d]: pos=(%.2f,%.2f) vel=%.2f\n",
               j, traj.points[j].config.x, traj.points[j].config.y,
               traj.points[j].velocity);

    mr_trajectory spline_traj;
    mr_trajectory_spline(&traj_path, 5, &spline_traj);
    printf("Spline trajectory: %d points\n", spline_traj.num_points);

    printf("\n--- Collision Detection ---\n");

    mr_point2d test_points[5] = {
        {4.9, 4.9}, {5.5, 5.5}, {1.0, 1.0}, {3.0, 7.0}, {7.0, 3.0}
    };
    for (j = 0; j < 5; j++) {
        int coll = mr_collision_check_point(&test_points[j], &world);
        printf("  Point (%.1f, %.1f): %s\n",
               test_points[j].x, test_points[j].y,
               coll ? "COLLISION" : "free");
    }

    printf("\n--- Repulsive Potential Field ---\n");

    mr_point2d robot_pos = {2.5, 5.0};
    mr_point2d force;

    double pot = mr_repulsive_potential(&robot_pos, &world,
                                        0.5, 2.0, &force);
    printf("  Robot at (%.1f, %.1f): potential=%.4f, force=(%.4f, %.4f)\n",
           robot_pos.x, robot_pos.y, pot, force.x, force.y);

    robot_pos.x = 7.5; robot_pos.y = 8.0;
    pot = mr_repulsive_potential(&robot_pos, &world, 0.5, 2.0, &force);
    printf("  Robot at (%.1f, %.1f): potential=%.4f, force=(%.4f, %.4f)\n",
           robot_pos.x, robot_pos.y, pot, force.x, force.y);

    robot_pos.x = 5.0; robot_pos.y = 5.0;
    pot = mr_repulsive_potential(&robot_pos, &world, 0.5, 2.0, &force);
    printf("  Robot at (%.1f, %.1f): potential=%.4f, force=(%.4f, %.4f)\n",
           robot_pos.x, robot_pos.y, pot, force.x, force.y);

    printf("\nDone.\n");
    return 0;
}
