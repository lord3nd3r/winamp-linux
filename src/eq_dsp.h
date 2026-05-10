// eq_dsp.h — EQ10 DSP Engine
// Ported from Winamp's eq10dsp.cpp / eq10dsp.h
// Original: Copyright (C) 2002 4Front Technologies, by George Yohng
// 10-band graphic equalizer with asymmetric Q (narrow boost, wide cut)
// and dynamic limiter. This is the REAL Winamp EQ algorithm.
#pragma once

#include <cmath>
#include <cstring>

#define EQ10_NOFBANDS 10
#define EQ10_Q        1.41   // global Q factor (matches original)
#define EQ10_TRIM_CODE    0.930  // limiter trim at -0.6dB
#define EQ10_TRIM_RELEASE 0.700  // limiter release time in seconds

struct eq10band_t {
    double gain;
    double ua0, ub1, ub2;  // "up" coefficients (boost, narrow Q)
    double da0, db1, db2;  // "down" coefficients (cut, wide Q)
    double x1, x2, y1, y2; // filter state
};

struct eq10_t {
    double rate;
    eq10band_t band[EQ10_NOFBANDS];
    double detect;       // limiter peak detector
    double detectdecay;  // limiter release coefficient
};

// Winamp-style frequency table (matches eq10dsp.cpp line 28)
static const double eq10_freq[EQ10_NOFBANDS] = {
    70, 180, 320, 600, 1000, 3000, 6000, 12000, 14000, 16000
};

// Preamp lookup table — 64 entries mapping slider 0-63 to linear gain
// (matches In.cpp eq_lookup1[64]: index 0 = +12dB = 4.0x, 31 = 0dB = 1.0x, 63 = -12dB = 0.25x)
static const float eq_preamp_table[64] = {
    4.000000f, 3.610166f, 3.320019f, 3.088821f, 2.896617f,
    2.732131f, 2.588368f, 2.460685f, 2.345845f, 2.241498f,
    2.145887f, 2.057660f, 1.975760f, 1.899338f, 1.827707f,
    1.760303f, 1.696653f, 1.636363f, 1.579094f, 1.524558f,
    1.472507f, 1.422724f, 1.375019f, 1.329225f, 1.285197f,
    1.242801f, 1.201923f, 1.162456f, 1.124306f, 1.087389f,
    1.051628f, 1.000000f, 0.983296f, 0.950604f, 0.918821f,
    0.887898f, 0.857789f, 0.828454f, 0.799853f, 0.771950f,
    0.744712f, 0.718108f, 0.692110f, 0.666689f, 0.641822f,
    0.617485f, 0.593655f, 0.570311f, 0.547435f, 0.525008f,
    0.503013f, 0.481433f, 0.460253f, 0.439458f, 0.419035f,
    0.398970f, 0.379252f, 0.359868f, 0.340807f, 0.322060f,
    0.303614f, 0.285462f, 0.267593f, 0.250000f
};

// Slider value (0-63) to dB (matches In.cpp VALTODB)
// Slider pos: 63 = top = +12dB boost, 31 = center = 0dB, 0 = bottom = -12dB cut
static inline double eq10_valtodb(int v) {
    v = 31 - v;  // Flip: 63→-32 (boost), 0→+31 (cut)
    if (v < -31) v = -31;
    if (v > 32) v = 32;
    if (v > 0) return -12.0 * (v / 32.0);
    else if (v < 0) return -12.0 * (v / 31.0);
    return 0.0;
}

// dB to internal gain value (matches eq10dsp.cpp eq10_db2gain)
static inline double eq10_db2gain(double gain_dB) {
    return pow(10.0, gain_dB / 20.0) - 1.0;
}

// Setup bandpass coefficients for one direction (boost or cut)
static inline void eq10_bsetup2(int u, double rate, eq10band_t *band, double freq, double Q) {
    if (rate < 4000.0) rate = 4000.0;
    if (rate > 384000.0) rate = 384000.0;
    if (freq < 20.0) freq = 20.0;
    if (freq >= (rate * 0.499)) { band->ua0 = band->da0 = 0; return; }

    double angle = 2.0 * M_PI * freq / rate;
    double alpha = sin(angle) / (2.0 * Q);

    double b0 = 1.0 / (1.0 + alpha);
    double a0 = b0 * alpha;
    double b1 = b0 * 2 * cos(angle);
    double b2 = b0 * (alpha - 1);

    if (u > 0) { band->ua0 = a0; band->ub1 = b1; band->ub2 = b2; }
    else       { band->da0 = a0; band->db1 = b1; band->db2 = b2; }
}

// Setup one band: wide Q for cut (Q*0.5), narrow Q for boost (Q*2.0)
static inline void eq10_bsetup(double rate, eq10band_t *band, double freq, double Q) {
    memset(band, 0, sizeof(*band));
    eq10_bsetup2(-1, rate, band, freq, Q * 0.5);
    eq10_bsetup2(+1, rate, band, freq, Q * 2.0);
}

// Initialize EQ for all channels
static inline void eq10_setup(eq10_t *eq, int eqs, double rate) {
    for (int k = 0; k < eqs; k++, eq++) {
        eq->rate = rate;
        for (int t = 0; t < EQ10_NOFBANDS; t++)
            eq10_bsetup(rate, &eq->band[t], eq10_freq[t], EQ10_Q);
        eq->detect = 0;
        eq->detectdecay = pow(0.001, 1.0 / (rate * EQ10_TRIM_RELEASE));
    }
}

// Set gain for a band across all channels
static inline void eq10_setgain(eq10_t *eq, int eqs, int bandnr, double gain_dB) {
    double realgain = eq10_db2gain(gain_dB);
    for (int k = 0; k < eqs; k++)
        eq[k].band[bandnr].gain = realgain;
}

// Process float samples through one channel's EQ (matches eq10dsp.cpp eq10_processf)
// buf = input (interleaved), outbuf = output, sz = frame count, idx = channel, step = channel count
static inline void eq10_processf(eq10_t *eq, float *buf, float *outbuf, int sz, int idx, int step) {
    if (!eq) return;
    buf += idx;
    outbuf += idx;
    float *in = buf;

    for (int k = 0; k < EQ10_NOFBANDS; k++) {
        double a0, b1, b2;
        double x1 = eq->band[k].x1, x2 = eq->band[k].x2;
        double y1 = eq->band[k].y1, y2 = eq->band[k].y2;
        double gain = eq->band[k].gain;
        float *out = outbuf;

        if (gain > 0.0) {
            a0 = eq->band[k].ua0 * gain;
            b1 = eq->band[k].ub1;
            b2 = eq->band[k].ub2;
        } else {
            a0 = eq->band[k].da0 * gain;
            b1 = eq->band[k].db1;
            b2 = eq->band[k].db2;
        }

        if (a0 == 0.0) continue;

        for (int t = 0; t < sz; t++, in += step, out += step) {
            double y0 = (in[0] - x2) * a0 + y1 * b1 + y2 * b2 + 1e-30; // denormal fix
            x2 = x1; x1 = in[0]; y2 = y1; y1 = y0;
            out[0] = (float)(y0 + in[0]); // parallel bandpass topology
        }
        in = outbuf; // chain bands serially
        eq->band[k].x1 = x1; eq->band[k].x2 = x2;
        eq->band[k].y1 = y1; eq->band[k].y2 = y2;
    }

    // Dynamic limiter (matches eq10dsp.cpp)
    {
        double detect = eq->detect;
        double detectdecay = eq->detectdecay;
        float *out = outbuf;
        for (int t = 0; t < sz; t++, in += step, out += step) {
            if (fabs(in[0]) > detect) detect = fabs(in[0]);
            if (detect > EQ10_TRIM_CODE)
                out[0] = in[0] * (float)(EQ10_TRIM_CODE / detect);
            else
                out[0] = in[0];
            detect *= detectdecay;
            detect += 1e-30; // denormal fix
        }
        eq->detect = detect;
    }
}
