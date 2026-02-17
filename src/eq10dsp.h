// ========================================================================
// EQ10 DSP Engine — ported from Winamp's eq10dsp.cpp / eq10dsp.h
// Original: Copyright (C) 2002 4Front Technologies, by George Yohng
// 10-band graphic equalizer with asymmetric Q (narrow boost, wide cut)
// and dynamic limiter. This is the REAL Winamp EQ algorithm.
// ========================================================================
#ifndef EQ10DSP_H
#define EQ10DSP_H

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
extern const double eq10_freq[EQ10_NOFBANDS];

// Preamp lookup table — 64 entries mapping slider 0-63 to linear gain
extern const float eq_preamp_table[64];

// Slider value (0-63) to dB (matches In.cpp VALTODB)
double eq10_valtodb(int v);

// dB to internal gain value
double eq10_db2gain(double gain_dB);

// Initialize EQ for all channels
void eq10_setup(eq10_t *eq, int eqs, double rate);

// Set gain for a band across all channels
void eq10_setgain(eq10_t *eq, int eqs, int bandnr, double gain_dB);

// Process float samples through one channel's EQ
void eq10_processf(eq10_t *eq, float *buf, float *outbuf, int sz, int idx, int step);

#endif // EQ10DSP_H
