#include "kinematics.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

mr_mat4 mr_dh_transform(const mr_dh_param *param)
{
    double ct = cos(param->theta), st = sin(param->theta);
    double ca = cos(param->alpha), sa = sin(param->alpha);
    mr_mat4 t;

    t.m[0][0] = ct;
    t.m[0][1] = -st * ca;
    t.m[0][2] = st * sa;
    t.m[0][3] = param->a * ct;

    t.m[1][0] = st;
    t.m[1][1] = ct * ca;
    t.m[1][2] = -ct * sa;
    t.m[1][3] = param->a * st;

    t.m[2][0] = 0.0;
    t.m[2][1] = sa;
    t.m[2][2] = ca;
    t.m[2][3] = param->d;

    t.m[3][0] = 0.0;
    t.m[3][1] = 0.0;
    t.m[3][2] = 0.0;
    t.m[3][3] = 1.0;

    return t;
}

mr_mat4 mr_mat4_mul(const mr_mat4 *a, const mr_mat4 *b)
{
    mr_mat4 r;
    int i, j, k;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            r.m[i][j] = 0.0;
            for (k = 0; k < 4; k++) {
                r.m[i][j] += a->m[i][k] * b->m[k][j];
            }
        }
    }
    return r;
}

mr_mat4 mr_mat4_inv(const mr_mat4 *m)
{
    mr_mat4 inv;
    double R[3][3], t[3];
    int i;

    for (i = 0; i < 3; i++) {
        R[i][0] = m->m[i][0];
        R[i][1] = m->m[i][1];
        R[i][2] = m->m[i][2];
        t[i] = m->m[i][3];
    }

    for (i = 0; i < 3; i++) {
        inv.m[i][0] = R[0][i];
        inv.m[i][1] = R[1][i];
        inv.m[i][2] = R[2][i];
        inv.m[i][3] = -(R[0][i] * t[0] + R[1][i] * t[1] + R[2][i] * t[2]);
    }

    inv.m[3][0] = 0.0;
    inv.m[3][1] = 0.0;
    inv.m[3][2] = 0.0;
    inv.m[3][3] = 1.0;

    return inv;
}

mr_mat4 mr_mat4_identity(void)
{
    mr_mat4 m;
    int i, j;

    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            m.m[i][j] = (i == j) ? 1.0 : 0.0;

    return m;
}

mr_pose mr_forward_kinematics(const mr_robot_desc *robot,
                              const double *q, int n)
{
    mr_mat4 t = mr_mat4_identity();
    mr_mat4 base_tf;
    mr_pose pose;
    int i;

    mr_pose_to_mat4(&robot->base_pose, &base_tf);
    t = mr_mat4_mul(&t, &base_tf);

    for (i = 0; i < n && i < robot->num_joints; i++) {
        mr_dh_param dh = robot->joints[i].dh;
        if (robot->joints[i].type == MR_JOINT_REVOLUTE)
            dh.theta += q[i];
        else
            dh.d += q[i];
        mr_mat4 ai = mr_dh_transform(&dh);
        t = mr_mat4_mul(&t, &ai);
    }

    mr_mat4_to_pose(&t, &pose);
    return pose;
}

void mr_forward_kinematics_chain(const mr_robot_desc *robot,
                                 const double *q, int n,
                                 mr_mat4 *frames_out)
{
    int i;

    for (i = 0; i < n && i < robot->num_joints; i++) {
        mr_dh_param dh = robot->joints[i].dh;
        if (robot->joints[i].type == MR_JOINT_REVOLUTE)
            dh.theta += q[i];
        else
            dh.d += q[i];
        mr_mat4 ai = mr_dh_transform(&dh);
        if (i == 0)
            frames_out[i] = ai;
        else
            frames_out[i] = mr_mat4_mul(&frames_out[i - 1], &ai);
    }
}

int mr_inverse_kinematics_numerical(const mr_robot_desc *robot,
                                    const mr_pose *target,
                                    const double *q0, int n,
                                    double *q_out)
{
    double q[MR_MAX_JOINTS];
    mr_jacobian J;
    double err;
    int iter, i;

    memcpy(q, q0, n * sizeof(double));

    for (iter = 0; iter < MR_MAX_IK_ITERS; iter++) {
        mr_pose current = mr_forward_kinematics(robot, q, n);

        double dx = target->x - current.x;
        double dy = target->y - current.y;
        double dz = target->z - current.z;
        double dr = target->roll - current.roll;
        double dp = target->pitch - current.pitch;
        double dyaw = target->yaw - current.yaw;

        err = sqrt(dx * dx + dy * dy + dz * dz + dr * dr + dp * dp + dyaw * dyaw);
        if (err < MR_IK_TOL) {
            memcpy(q_out, q, n * sizeof(double));
            return 1;
        }

        mr_build_jacobian(robot, q, n, &J);

        double pinv[MR_MAX_JOINTS][6];
        double e[6] = {dx, dy, dz, dr, dp, dyaw};

        if (mr_jacobian_pinv(&J, 0.01, pinv)) {
            double dq[MR_MAX_JOINTS] = {0};
            for (i = 0; i < n; i++)
                for (int j = 0; j < 6; j++)
                    dq[i] += pinv[i][j] * e[j];
            for (i = 0; i < n; i++) {
                q[i] += dq[i];
                if (q[i] < robot->joints[i].q_min) q[i] = robot->joints[i].q_min;
                if (q[i] > robot->joints[i].q_max) q[i] = robot->joints[i].q_max;
            }
        } else {
            break;
        }
    }
    memcpy(q_out, q, n * sizeof(double));
    return 0;
}

void mr_build_jacobian(const mr_robot_desc *robot,
                       const double *q, int n,
                       mr_jacobian *J)
{
    mr_mat4 frames[MR_MAX_JOINTS];
    mr_pose end_effector;
    int i;

    mr_forward_kinematics_chain(robot, q, n, frames);
    mr_mat4 Tn = (n > 0) ? frames[n - 1] : mr_mat4_identity();
    mr_mat4_to_pose(&Tn, &end_effector);

    J->rows = 6;
    J->cols = n;

    for (i = 0; i < n; i++) {
        mr_mat4 ti = (i == 0) ? frames[0] : mr_mat4_mul(&frames[i - 1], &mr_mat4_identity());
        double zx = ti.m[0][2], zy = ti.m[1][2], zz = ti.m[2][2];
        double px = ti.m[0][3], py = ti.m[1][3], pz = ti.m[2][3];

        if (robot->joints[i].type == MR_JOINT_REVOLUTE) {
            J->m[0][i] = -(zy * (end_effector.z - pz) - zz * (end_effector.y - py));
            J->m[1][i] = -(zz * (end_effector.x - px) - zx * (end_effector.z - pz));
            J->m[2][i] = -(zx * (end_effector.y - py) - zy * (end_effector.x - px));
            J->m[3][i] = zx;
            J->m[4][i] = zy;
            J->m[5][i] = zz;
        } else {
            J->m[0][i] = zx;
            J->m[1][i] = zy;
            J->m[2][i] = zz;
            J->m[3][i] = 0.0;
            J->m[4][i] = 0.0;
            J->m[5][i] = 0.0;
        }
    }
}

int mr_jacobian_pinv(const mr_jacobian *J, double damping,
                     double pinv[MR_MAX_JOINTS][6])
{
    int n = J->cols, m = J->rows;
    double JJt[6][6] = {{0}};
    int i, j, k;
    double det;

    for (i = 0; i < m; i++)
        for (j = 0; j < m; j++)
            for (k = 0; k < n; k++)
                JJt[i][j] += J->m[i][k] * J->m[j][k];

    for (i = 0; i < m; i++)
        JJt[i][i] += damping * damping;

    det = JJt[0][0] * (JJt[1][1] * JJt[2][2] - JJt[1][2] * JJt[2][1])
        - JJt[0][1] * (JJt[1][0] * JJt[2][2] - JJt[1][2] * JJt[2][0])
        + JJt[0][2] * (JJt[1][0] * JJt[2][1] - JJt[1][1] * JJt[2][0]);

    if (fabs(det) < 1e-12) return 0;

    double invJJt[6][6];
    double invdet = 1.0 / det;

    invJJt[0][0] =  (JJt[1][1] * JJt[2][2] - JJt[1][2] * JJt[2][1]) * invdet;
    invJJt[0][1] = -(JJt[0][1] * JJt[2][2] - JJt[0][2] * JJt[2][1]) * invdet;
    invJJt[0][2] =  (JJt[0][1] * JJt[1][2] - JJt[0][2] * JJt[1][1]) * invdet;
    invJJt[1][0] = -(JJt[1][0] * JJt[2][2] - JJt[1][2] * JJt[2][0]) * invdet;
    invJJt[1][1] =  (JJt[0][0] * JJt[2][2] - JJt[0][2] * JJt[2][0]) * invdet;
    invJJt[1][2] = -(JJt[0][0] * JJt[1][2] - JJt[0][2] * JJt[1][0]) * invdet;
    invJJt[2][0] =  (JJt[1][0] * JJt[2][1] - JJt[1][1] * JJt[2][0]) * invdet;
    invJJt[2][1] = -(JJt[0][0] * JJt[2][1] - JJt[0][1] * JJt[2][0]) * invdet;
    invJJt[2][2] =  (JJt[0][0] * JJt[1][1] - JJt[0][1] * JJt[1][0]) * invdet;

    for (i = 0; i < n; i++)
        for (j = 0; j < m; j++) {
            pinv[i][j] = 0.0;
            for (k = 0; k < m; k++)
                pinv[i][j] += J->m[k][i] * invJJt[k][j];
        }

    return 1;
}

void mr_pose_to_mat4(const mr_pose *pose, mr_mat4 *m)
{
    double cr = cos(pose->roll), sr = sin(pose->roll);
    double cp = cos(pose->pitch), sp = sin(pose->pitch);
    double cy = cos(pose->yaw), sy = sin(pose->yaw);

    *m = mr_mat4_identity();

    m->m[0][0] = cp * cy;
    m->m[0][1] = sr * sp * cy - cr * sy;
    m->m[0][2] = cr * sp * cy + sr * sy;
    m->m[0][3] = pose->x;

    m->m[1][0] = cp * sy;
    m->m[1][1] = sr * sp * sy + cr * cy;
    m->m[1][2] = cr * sp * sy - sr * cy;
    m->m[1][3] = pose->y;

    m->m[2][0] = -sp;
    m->m[2][1] = sr * cp;
    m->m[2][2] = cr * cp;
    m->m[2][3] = pose->z;
}

void mr_mat4_to_pose(const mr_mat4 *m, mr_pose *pose)
{
    pose->x = m->m[0][3];
    pose->y = m->m[1][3];
    pose->z = m->m[2][3];

    pose->yaw = atan2(m->m[1][0], m->m[0][0]);
    pose->pitch = atan2(-m->m[2][0],
                        sqrt(m->m[2][1] * m->m[2][1] + m->m[2][2] * m->m[2][2]));
    pose->roll = atan2(m->m[2][1], m->m[2][2]);
}

double mr_pose_distance(const mr_pose *a, const mr_pose *b)
{
    double dx = a->x - b->x;
    double dy = a->y - b->y;
    double dz = a->z - b->z;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

int mr_workspace_check(const mr_robot_desc *robot, const mr_pose *point)
{
    int i;
    double q[MR_MAX_JOINTS] = {0};

    for (i = 0; i < robot->num_joints; i++)
        q[i] = robot->joints[i].q_min;

    mr_pose low = mr_forward_kinematics(robot, q, robot->num_joints);

    for (i = 0; i < robot->num_joints; i++)
        q[i] = robot->joints[i].q_max;

    mr_pose high = mr_forward_kinematics(robot, q, robot->num_joints);

    if (point->x < low.x - 0.01 || point->x > high.x + 0.01) return 0;
    if (point->y < low.y - 0.01 || point->y > high.y + 0.01) return 0;
    if (point->z < low.z - 0.01 || point->z > high.z + 0.01) return 0;

    return 1;
}

void mr_workspace_bounds(const mr_robot_desc *robot,
                         mr_pose *min_bounds, mr_pose *max_bounds)
{
    int i, j;
    double q[MR_MAX_JOINTS];
    mr_pose p;

    min_bounds->x = min_bounds->y = min_bounds->z = 1e9;
    max_bounds->x = max_bounds->y = max_bounds->z = -1e9;

    int samples[3] = {0, robot->joints[0].q_max != robot->joints[0].q_min ? 5 : 1,
                       robot->num_joints > 1 ? 5 : 1};

    for (i = 0; i < samples[0] || i == 0; i++) {
        for (j = 0; j < robot->num_joints; j++) {
            double range = robot->joints[j].q_max - robot->joints[j].q_min;
            q[j] = robot->joints[j].q_min + range * (double)i / ((double)(samples[0] > 0 ? samples[0] : 1));
        }
        p = mr_forward_kinematics(robot, q, robot->num_joints);
        if (p.x < min_bounds->x) min_bounds->x = p.x;
        if (p.y < min_bounds->y) min_bounds->y = p.y;
        if (p.z < min_bounds->z) min_bounds->z = p.z;
        if (p.x > max_bounds->x) max_bounds->x = p.x;
        if (p.y > max_bounds->y) max_bounds->y = p.y;
        if (p.z > max_bounds->z) max_bounds->z = p.z;
    }
}

double mr_manipulability(const mr_jacobian *J)
{
    double JJt[6][6] = {{0}};
    int i, j, k;
    double val = 0.0;

    for (i = 0; i < 6 && i < J->rows; i++)
        for (j = 0; j < 6 && j < J->rows; j++)
            for (k = 0; k < J->cols; k++)
                JJt[i][j] += J->m[i][k] * J->m[j][k];

    for (i = 0; i < 6 && i < J->rows; i++)
        val += JJt[i][i];

    return sqrt(val > 0 ? val : 0.0);
}

int mr_inverse_kinematics_analytical(const mr_robot_desc *robot,
                                     const mr_pose *target,
                                     int max_solutions,
                                     double solutions[][MR_MAX_JOINTS])
{
    if (robot->num_joints != 3) return 0;

    double arm_len1 = robot->joints[1].dh.a;
    double arm_len2 = robot->joints[2].dh.a;

    double wx = target->x - robot->joints[0].dh.a * cos(target->yaw);
    double wy = target->y - robot->joints[0].dh.a * sin(target->yaw);
    double wz = target->z - robot->joints[0].dh.d;

    double r = sqrt(wx * wx + wy * wy);
    double s = wz;
    double d = sqrt(r * r + s * s);

    if (d > arm_len1 + arm_len2 + 0.001 || d < fabs(arm_len1 - arm_len2) - 0.001)
        return 0;

    double cos_elbow = (d * d - arm_len1 * arm_len1 - arm_len2 * arm_len2)
                       / (2.0 * arm_len1 * arm_len2);
    if (cos_elbow < -1.0) cos_elbow = -1.0;
    if (cos_elbow > 1.0) cos_elbow = 1.0;

    double elbow_up = acos(cos_elbow);
    double elbow_down = -elbow_up;

    double base_angle = atan2(wy, wx);
    double shoulder_base = atan2(s, r);
    double shoulder_offset_up = atan2(arm_len2 * sin(elbow_up),
                                      arm_len1 + arm_len2 * cos(elbow_up));
    double shoulder_offset_down = atan2(arm_len2 * sin(elbow_down),
                                        arm_len1 + arm_len2 * cos(elbow_down));

    int count = 0;

    if (count < max_solutions) {
        solutions[count][0] = base_angle;
        solutions[count][1] = shoulder_base + shoulder_offset_up;
        solutions[count][2] = elbow_up;
        count++;
    }
    if (count < max_solutions) {
        solutions[count][0] = base_angle;
        solutions[count][1] = shoulder_base + shoulder_offset_down;
        solutions[count][2] = elbow_down;
        count++;
    }

    return count;
}
