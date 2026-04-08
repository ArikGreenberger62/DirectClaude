/* calibration.h — Two-position rotation-matrix calibration interface
 *
 * Algorithm:
 *  Position 1 (board stable): average N samples → gravity vector g1 (sensor frame)
 *  User rotates board 90° (red LED blinks fast as cue).
 *  Position 2 (stable again): average N samples → gravity vector g2 (sensor frame)
 *
 *  Build orthonormal basis:
 *    e3 = normalize(g1)          ← world Z (gravity / "down")
 *    e2 = normalize(e3 × g2)     ← world Y
 *    e1 = e2 × e3                ← world X
 *
 *  Rotation matrix R (rows = e1, e2, e3) maps sensor → world frame.
 *  Calibrated output = R * raw_vector, scaled so 1024 = 1G.
 */
#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdint.h>

/* ── Calibration states ──────────────────────────────────────────────────── */
typedef enum
{
    CAL_STATE_SAMPLE_POS1,   /* collecting stable samples at position 1 */
    CAL_STATE_WAIT_MOVE,     /* red LED fast blink, waiting for board to move */
    CAL_STATE_WAIT_STABLE2,  /* board moved, now waiting for it to settle */
    CAL_STATE_SAMPLE_POS2,   /* collecting stable samples at position 2 */
    CAL_STATE_COMPUTE,       /* computing rotation matrix */
    CAL_STATE_DONE,          /* calibration valid — green LED blinks */
    CAL_STATE_FAILED         /* calibration invalid — retry after delay */
} CalState_t;

/* ── Public API ──────────────────────────────────────────────────────────── */

/** Initialise / reset calibration state machine. */
void Calib_Init(void);

/** Return current calibration state. */
CalState_t Calib_GetState(void);

/**
 * @brief  Feed one accelerometer sample into the state machine.
 *         Call at ≥10 Hz.  Drives all state transitions internally.
 * @param  rx, ry, rz  Raw 16-bit accel counts from LSM6DSO.
 */
void Calib_Feed(int16_t rx, int16_t ry, int16_t rz);

/**
 * @brief  Apply the calibrated rotation matrix to a raw sample.
 *         Valid only when Calib_GetState() == CAL_STATE_DONE.
 * @param  rx, ry, rz    Raw 16-bit input counts.
 * @param  cx, cy, cz    Output counts scaled so 1024 = 1G.
 */
void Calib_Apply(int16_t rx, int16_t ry, int16_t rz,
                 int32_t *cx, int32_t *cy, int32_t *cz);

/** Force calibration to restart from position 1 sampling. */
void Calib_Restart(void);

#endif /* CALIBRATION_H */
