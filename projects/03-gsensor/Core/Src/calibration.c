/* calibration.c — Two-position rotation-matrix calibration
 *
 * Uses single-precision float for matrix math (Cortex-M33 FPU, hard-float ABI).
 * All float arithmetic is isolated here; main.c and lsm6dso.c are float-free.
 *
 * Scale convention throughout this module:
 *   RAW counts (int16_t): 1G ≈ 4096 at ±8G full scale
 *   CALIBRATED output (int32_t): 1024 = 1G  (raw / 4)
 */

#include "calibration.h"
#include <stdlib.h>  /* abs */
/* Use GCC built-ins for FPU single-cycle ops — avoids libm __errno linkage issue
 * with nano.specs.  __builtin_sqrtf emits VSQRT.F32 on Cortex-M33 FPU.      */
#define sqrtf(x)  __builtin_sqrtf(x)
#define fabsf(x)  __builtin_fabsf(x)

/* ── Tuning constants ────────────────────────────────────────────────────── */

/* Number of samples to average per position */
#define CAL_SAMPLES       32U

/* Stability criterion: range of magnitude over the sample window must be < this */
#define STABLE_THRESH     80    /* ~20 mg at 4096 counts/G */

/* Movement detection: direction threshold — cos(θ) < this triggers transition.
 * 0.9 ≈ cos(26°): board must rotate at least 26° before WAIT_STABLE2 begins. */
#define MOVE_COS_THRESH   0.9f

/* Minimum cross-product magnitude (normalised) to accept the calibration.
 * cos(60°) = 0.5 → sin(60°) ≈ 0.87.  Reject if rotation angle < ~30°. */
#define MIN_CROSS_MAG     0.5f

/* 1G in raw counts (±8G, 16-bit: 32768/8 = 4096) */
#define ONE_G_RAW         4096

/* Scale from raw to output units: output = raw / 4 */
#define RAW_TO_OUTPUT(r)  ((int32_t)(r) >> 2)

/* ── Internal state ──────────────────────────────────────────────────────── */

static CalState_t s_state = CAL_STATE_SAMPLE_POS1;

/* Accumulators for averaging */
static int32_t  s_acc_x, s_acc_y, s_acc_z;
static uint32_t s_acc_count;

/* Min/max magnitude within the current window for stability check */
static int32_t  s_mag_min, s_mag_max;

/* Stored gravity vectors (float, raw-count scale) */
static float s_g1[3];
static float s_g2[3];

/* Rotation matrix rows [3][3] */
static float s_R[3][3];

/* Flags */
static uint8_t s_moved;         /* 1 once movement detected after pos1 */

/* ── Private helpers ─────────────────────────────────────────────────────── */

static int32_t isqrt32(int64_t n)
{
    /* Integer square root used only for magnitude comparison (no float needed) */
    if (n <= 0) { return 0; }
    int64_t x = n;
    int64_t y = (x + 1) / 2;
    while (y < x)
    {
        x = y;
        y = (x + n / x) / 2;
    }
    return (int32_t)x;
}

static void reset_accumulators(void)
{
    s_acc_x = 0; s_acc_y = 0; s_acc_z = 0;
    s_acc_count = 0U;
    s_mag_min = 0x7FFFFFFF;
    s_mag_max = 0;
}

static int32_t sample_magnitude(int16_t rx, int16_t ry, int16_t rz)
{
    int64_t sq = (int64_t)rx*rx + (int64_t)ry*ry + (int64_t)rz*rz;
    return isqrt32(sq);
}

/* Cross product c = a × b */
static void cross3f(const float *a, const float *b, float *c)
{
    c[0] = a[1]*b[2] - a[2]*b[1];
    c[1] = a[2]*b[0] - a[0]*b[2];
    c[2] = a[0]*b[1] - a[1]*b[0];
}

static float norm3f(const float *v)
{
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static void normalize3f(const float *v, float *out)
{
    float n = norm3f(v);
    if (n < 1e-6f) { out[0] = 0.f; out[1] = 0.f; out[2] = 1.f; return; }
    out[0] = v[0]/n; out[1] = v[1]/n; out[2] = v[2]/n;
}

/* ── Calibration matrix compute ──────────────────────────────────────────── */

/* Returns 1 on success, 0 on failure */
static uint8_t compute_rotation_matrix(void)
{
    float e3[3], e2_raw[3], e2[3], e1[3];

    /* e3 = normalize(g1) — this is the "down" direction */
    normalize3f(s_g1, e3);

    /* e2_raw = e3 × g2 */
    cross3f(e3, s_g2, e2_raw);

    /* Reject if g1 and g2 are nearly parallel (user didn't rotate ~90°) */
    if (norm3f(e2_raw) < MIN_CROSS_MAG * norm3f(s_g2))
    {
        return 0U;
    }

    /* e2 = normalize(e3 × g2) */
    normalize3f(e2_raw, e2);

    /* e1 = e2 × e3 (right-hand rule) */
    cross3f(e2, e3, e1);

    /* Build rotation matrix: rows are [e1, e2, e3] */
    s_R[0][0] = e1[0]; s_R[0][1] = e1[1]; s_R[0][2] = e1[2];
    s_R[1][0] = e2[0]; s_R[1][1] = e2[1]; s_R[1][2] = e2[2];
    s_R[2][0] = e3[0]; s_R[2][1] = e3[1]; s_R[2][2] = e3[2];

    /* Sanity: det(R) should be ≈ 1.0 for a proper rotation */
    float det = s_R[0][0]*(s_R[1][1]*s_R[2][2] - s_R[1][2]*s_R[2][1])
              - s_R[0][1]*(s_R[1][0]*s_R[2][2] - s_R[1][2]*s_R[2][0])
              + s_R[0][2]*(s_R[1][0]*s_R[2][1] - s_R[1][1]*s_R[2][0]);

    if (fabsf(det - 1.0f) > 0.1f)
    {
        return 0U;
    }

    /* Self-test: R * g1 must produce a Z-dominant result.
     * Compute Rz (third row) · g1_norm; should be ≈ 1.0 */
    float g1_norm[3];
    normalize3f(s_g1, g1_norm);
    float rz_dot_g1 = e3[0]*g1_norm[0] + e3[1]*g1_norm[1] + e3[2]*g1_norm[2];
    if (fabsf(rz_dot_g1 - 1.0f) > 0.1f)
    {
        return 0U;
    }

    return 1U;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void Calib_Init(void)
{
    s_state   = CAL_STATE_SAMPLE_POS1;
    s_moved   = 0U;
    reset_accumulators();
}

CalState_t Calib_GetState(void)
{
    return s_state;
}

void Calib_Feed(int16_t rx, int16_t ry, int16_t rz)
{
    int32_t mag = sample_magnitude(rx, ry, rz);

    switch (s_state)
    {
    /* ── Position 1: collect stable samples ────────────────────────────── */
    case CAL_STATE_SAMPLE_POS1:
        s_acc_x += (int32_t)rx;
        s_acc_y += (int32_t)ry;
        s_acc_z += (int32_t)rz;
        s_acc_count++;

        if (mag < s_mag_min) { s_mag_min = mag; }
        if (mag > s_mag_max) { s_mag_max = mag; }

        if (s_acc_count >= CAL_SAMPLES)
        {
            /* Check stability */
            if ((s_mag_max - s_mag_min) < STABLE_THRESH)
            {
                /* Good — save average as g1 */
                s_g1[0] = (float)s_acc_x / (float)CAL_SAMPLES;
                s_g1[1] = (float)s_acc_y / (float)CAL_SAMPLES;
                s_g1[2] = (float)s_acc_z / (float)CAL_SAMPLES;
                s_state = CAL_STATE_WAIT_MOVE;
                s_moved = 0U;
            }
            /* Whether stable or not, reset and keep accumulating */
            reset_accumulators();
        }
        break;

    /* ── Wait for board to move (user rotating 90°) ─────────────────────── */
    case CAL_STATE_WAIT_MOVE:
    {
        /* Rotation changes gravity *direction*, not magnitude — magnitude
         * comparison is useless here.  Instead compare direction via dot
         * product: dot(current, g1) / (|current|*|g1|) = cos(θ).
         * When θ > ~26° (cos < 0.9), the board has moved.
         * Approximate |current| ≈ |g1| (gravity-only, slow rotation), so
         * threshold becomes: dot < 0.9 * g1_sq.                           */
        float fx    = (float)rx;
        float fy    = (float)ry;
        float fz    = (float)rz;
        float dot   = fx * s_g1[0] + fy * s_g1[1] + fz * s_g1[2];
        float g1_sq = s_g1[0]*s_g1[0] + s_g1[1]*s_g1[1] + s_g1[2]*s_g1[2];

        if (dot < MOVE_COS_THRESH * g1_sq)
        {
            s_moved = 1U;
        }
        if (s_moved)
        {
            s_state = CAL_STATE_WAIT_STABLE2;
            reset_accumulators();
        }
        break;
    }

    /* ── Wait for board to settle at position 2 ─────────────────────────── */
    case CAL_STATE_WAIT_STABLE2:
        s_acc_x += (int32_t)rx;
        s_acc_y += (int32_t)ry;
        s_acc_z += (int32_t)rz;
        s_acc_count++;

        if (mag < s_mag_min) { s_mag_min = mag; }
        if (mag > s_mag_max) { s_mag_max = mag; }

        if (s_acc_count >= CAL_SAMPLES)
        {
            if ((s_mag_max - s_mag_min) < STABLE_THRESH)
            {
                /* Stable at new position — transition to sampling */
                s_state = CAL_STATE_SAMPLE_POS2;
            }
            reset_accumulators();
        }
        break;

    /* ── Position 2: collect stable samples ────────────────────────────── */
    case CAL_STATE_SAMPLE_POS2:
        s_acc_x += (int32_t)rx;
        s_acc_y += (int32_t)ry;
        s_acc_z += (int32_t)rz;
        s_acc_count++;

        if (mag < s_mag_min) { s_mag_min = mag; }
        if (mag > s_mag_max) { s_mag_max = mag; }

        if (s_acc_count >= CAL_SAMPLES)
        {
            if ((s_mag_max - s_mag_min) < STABLE_THRESH)
            {
                s_g2[0] = (float)s_acc_x / (float)CAL_SAMPLES;
                s_g2[1] = (float)s_acc_y / (float)CAL_SAMPLES;
                s_g2[2] = (float)s_acc_z / (float)CAL_SAMPLES;
                s_state = CAL_STATE_COMPUTE;
            }
            reset_accumulators();
        }
        break;

    /* ── Compute rotation matrix ────────────────────────────────────────── */
    case CAL_STATE_COMPUTE:
        if (compute_rotation_matrix())
        {
            s_state = CAL_STATE_DONE;
        }
        else
        {
            s_state = CAL_STATE_FAILED;
        }
        break;

    /* ── Terminal states: handled by main ──────────────────────────────── */
    case CAL_STATE_DONE:
    case CAL_STATE_FAILED:
        break;

    default:
        break;
    }
}

void Calib_Apply(int16_t rx, int16_t ry, int16_t rz,
                 int32_t *cx, int32_t *cy, int32_t *cz)
{
    float fx = (float)rx;
    float fy = (float)ry;
    float fz = (float)rz;

    /* Apply rotation: world = R * sensor */
    float wx = s_R[0][0]*fx + s_R[0][1]*fy + s_R[0][2]*fz;
    float wy = s_R[1][0]*fx + s_R[1][1]*fy + s_R[1][2]*fz;
    float wz = s_R[2][0]*fx + s_R[2][1]*fy + s_R[2][2]*fz;

    /* Scale: raw 4096 = 1G → output 1024 = 1G (divide by 4) */
    *cx = (int32_t)(wx * 0.25f);
    *cy = (int32_t)(wy * 0.25f);
    *cz = (int32_t)(wz * 0.25f);
}

void Calib_Restart(void)
{
    s_state  = CAL_STATE_SAMPLE_POS1;
    s_moved  = 0U;
    reset_accumulators();
}
