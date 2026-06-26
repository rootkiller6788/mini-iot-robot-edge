#include "kinematics.h"
#include <stdio.h>
#include <math.h>

static void print_mat4(const char *label, const mr_mat4 *m)
{
    printf("%s:\n", label);
    int i, j;
    for (i = 0; i < 4; i++) {
        printf("  [");
        for (j = 0; j < 4; j++)
            printf(" % 8.4f", m->m[i][j]);
        printf(" ]\n");
    }
}

static void print_pose(const char *label, const mr_pose *p)
{
    printf("%s: pos=(%.3f, %.3f, %.3f) rpy=(%.3f, %.3f, %.3f)\n",
           label, p->x, p->y, p->z, p->roll, p->pitch, p->yaw);
}

int main(void)
{
    mr_robot_desc robot;
    robot.num_joints = 3;
    int i;

    for (i = 0; i < 3; i++) {
        robot.joints[i].type = MR_JOINT_REVOLUTE;
        robot.joints[i].q_min = -M_PI;
        robot.joints[i].q_max = M_PI;
    }

    robot.joints[0].dh = (mr_dh_param){0.0, -M_PI_2, 0.3, 0.0};
    robot.joints[1].dh = (mr_dh_param){0.5, 0.0, 0.0, 0.0};
    robot.joints[2].dh = (mr_dh_param){0.3, 0.0, 0.0, 0.0};

    printf("=== Forward Kinematics Demo ===\n\n");

    double configs[4][3] = {
        {0.0, 0.0, 0.0},
        {0.5, -0.8, 0.3},
        {1.2, 0.4, -0.9},
        {-0.7, 1.0, 0.5}
    };

    for (i = 0; i < 4; i++) {
        printf("--- Configuration %d: (%.2f, %.2f, %.2f) ---\n",
               i + 1, configs[i][0], configs[i][1], configs[i][2]);

        mr_pose pose = mr_forward_kinematics(&robot, configs[i], 3);
        print_pose("  End-effector", &pose);

        mr_mat4 frames[MR_MAX_JOINTS];
        mr_forward_kinematics_chain(&robot, configs[i], 3, frames);

        int j;
        for (j = 0; j < 3; j++) {
            char label[32];
            snprintf(label, sizeof(label), "  Frame[%d]", j);
            print_mat4(label, &frames[j]);
        }

        mr_workspace_check(&robot, &pose);
        printf("  In workspace: %s\n\n",
               mr_workspace_check(&robot, &pose) ? "yes" : "no");
    }

    printf("=== Workspace Bounds ===\n");
    mr_pose min_b, max_b;
    mr_workspace_bounds(&robot, &min_b, &max_b);
    printf("  Min: (%.3f, %.3f, %.3f)\n", min_b.x, min_b.y, min_b.z);
    printf("  Max: (%.3f, %.3f, %.3f)\n\n", max_b.x, max_b.y, max_b.z);

    printf("=== Inverse Kinematics (Numerical) ===\n");
    mr_pose target = {0.3, 0.2, 0.5, 0.0, 0.0, 0.0};
    print_pose("Target", &target);

    double q0[3] = {0.1, 0.1, 0.1};
    double q_sol[3];

    if (mr_inverse_kinematics_numerical(&robot, &target, q0, 3, q_sol)) {
        printf("  Solution found: (%.3f, %.3f, %.3f)\n",
               q_sol[0], q_sol[1], q_sol[2]);
        mr_pose actual = mr_forward_kinematics(&robot, q_sol, 3);
        print_pose("  Actual pose", &actual);
    } else {
        printf("  Numerical IK did not converge, using best guess\n");
        mr_pose actual = mr_forward_kinematics(&robot, q0, 3);
        print_pose("  Best guess pose", &actual);
    }

    printf("\n=== Analytical Inverse Kinematics (3-DOF planar) ===\n");
    mr_robot_desc planar;
    planar.num_joints = 3;
    planar.joints[0].type = MR_JOINT_REVOLUTE;
    planar.joints[0].dh = (mr_dh_param){0.0, 0.0, 0.0, 0.0};
    planar.joints[1].type = MR_JOINT_REVOLUTE;
    planar.joints[1].dh = (mr_dh_param){0.5, 0.0, 0.0, 0.0};
    planar.joints[2].type = MR_JOINT_REVOLUTE;
    planar.joints[2].dh = (mr_dh_param){0.3, 0.0, 0.0, 0.0};
    for (i = 0; i < 3; i++) {
        planar.joints[i].q_min = -M_PI;
        planar.joints[i].q_max = M_PI;
    }

    mr_pose planar_target = {0.6, 0.2, 0.0, 0.0, 0.0, 0.0};
    print_pose("Planar target", &planar_target);

    double solutions[4][MR_MAX_JOINTS];
    int nsol = mr_inverse_kinematics_analytical(&planar, &planar_target,
                                                 4, solutions);
    printf("  Found %d solutions:\n", nsol);
    for (i = 0; i < nsol; i++) {
        printf("    [%d]: (%.3f, %.3f, %.3f)\n",
               i, solutions[i][0], solutions[i][1], solutions[i][2]);
        mr_pose verify = mr_forward_kinematics(&planar, solutions[i], 3);
        print_pose("         verify", &verify);
    }

    printf("\n=== Jacobian Demo ===\n");
    double q_jac[3] = {0.5, 0.3, -0.2};
    mr_jacobian J;
    mr_build_jacobian(&robot, q_jac, 3, &J);
    printf("  Jacobian (%dx%d):\n", J.rows, J.cols);
    for (i = 0; i < J.rows; i++) {
        printf("  [");
        int j2;
        for (j2 = 0; j2 < J.cols; j2++)
            printf(" % 8.4f", J.m[i][j2]);
        printf(" ]\n");
    }

    double mu = mr_manipulability(&J);
    printf("\n  Manipulability measure: %.4f\n", mu);

    return 0;
}
