#ifndef MINI_ROBOTICS_KINEMATICS_H
#define MINI_ROBOTICS_KINEMATICS_H

#include <stddef.h>
#include <stdint.h>

#define MR_MAX_JOINTS 16
#define MR_MAX_IK_ITERS 500
#define MR_IK_TOL 1e-6

typedef struct {
    double a;     /* link length along x_i */
    double alpha; /* link twist about x_i */
    double d;     /* link offset along z_{i-1} */
    double theta; /* joint angle about z_{i-1} */
} mr_dh_param;

typedef struct {
    double m[4][4]; /* row-major */
} mr_mat4;

typedef struct {
    double x, y, z;
    double roll, pitch, yaw;
} mr_pose;

typedef struct {
    mr_pose pose;
    double arm_angle[MR_MAX_JOINTS]; /* joint configuration */
    double gripper;                   /* gripper opening (0=closed, 1=open) */
} mr_robot_state;

typedef struct {
    double v[3]; /* linear velocity */
    double w[3]; /* angular velocity */
} mr_twist;

typedef struct {
    double m[6][MR_MAX_JOINTS]; /* Jacobian: [v; w] = J * qdot */
    int rows;
    int cols;
} mr_jacobian;

typedef enum {
    MR_JOINT_REVOLUTE = 0,
    MR_JOINT_PRISMATIC = 1
} mr_joint_type;

typedef struct {
    mr_joint_type type;
    mr_dh_param dh;
    double q_min, q_max;   /* joint limits */
    double q;               /* current joint position */
} mr_joint;

typedef struct {
    mr_joint joints[MR_MAX_JOINTS];
    int num_joints;
    mr_pose base_pose;       /* world frame of base */
    mr_pose tool_pose;       /* tool frame relative to last link */
} mr_robot_desc;

mr_mat4 mr_dh_transform(const mr_dh_param *param);

mr_mat4 mr_mat4_mul(const mr_mat4 *a, const mr_mat4 *b);

mr_mat4 mr_mat4_inv(const mr_mat4 *m);

mr_mat4 mr_mat4_identity(void);

mr_pose mr_forward_kinematics(const mr_robot_desc *robot,
                              const double *q, int n);

void mr_forward_kinematics_chain(const mr_robot_desc *robot,
                                 const double *q, int n,
                                 mr_mat4 *frames_out);

int mr_inverse_kinematics_analytical(const mr_robot_desc *robot,
                                     const mr_pose *target,
                                     int max_solutions,
                                     double solutions[][MR_MAX_JOINTS]);

int mr_inverse_kinematics_numerical(const mr_robot_desc *robot,
                                    const mr_pose *target,
                                    const double *q0, int n,
                                    double *q_out);

void mr_build_jacobian(const mr_robot_desc *robot,
                       const double *q, int n,
                       mr_jacobian *J);

int mr_jacobian_pinv(const mr_jacobian *J, double damping,
                     double pinv[MR_MAX_JOINTS][6]);

void mr_pose_to_mat4(const mr_pose *pose, mr_mat4 *m);

void mr_mat4_to_pose(const mr_mat4 *m, mr_pose *pose);

double mr_pose_distance(const mr_pose *a, const mr_pose *b);

int mr_workspace_check(const mr_robot_desc *robot, const mr_pose *point);

void mr_workspace_bounds(const mr_robot_desc *robot,
                         mr_pose *min_bounds, mr_pose *max_bounds);

double mr_manipulability(const mr_jacobian *J);

#endif
