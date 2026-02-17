#include "eq10dsp.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Winamp-style frequency table (matches eq10dsp.cpp line 28)
const double eq10_freq[EQ10_NOFBANDS] = {
    70, 180, 320, 600, 1000, 3000, 6000, 12000, 14000, 16000
};

// Preamp lookup table — 64 entries mapping slider 0-63 to linear gain
const float eq_preamp_table[64] = {
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

double eq10_valtodb(int v) {
    v -= 31;
    if (v < -31) v = -31;
    if (v > 32) v = 32;
    if (v > 0) return -12.0 * (v / 32.0);
    else if (v < 0) return -12.0 * (v / 31.0);
    return 0.0;
}

double eq10_db2gain(double gain_dB) {
    return pow(10.0, gain_dB / 20.0) - 1.0;
}

static void eq10_bsetup2(int u, double rate, eq10band_t *band, double freq, double Q) {
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

static void eq10_bsetup(double rate, eq10band_t *band, double freq, double Q) {
    memset(band, 0, sizeof(*band));
    eq10_bsetup2(-1, rate, band, freq, Q * 0.5);
    eq10_bsetup2(+1, rate, band, freq, Q * 2.0);
}

void eq10_setup(eq10_t *eq, int eqs, double rate) {
    for (int k = 0; k < eqs; k++, eq++) {
        eq->rate = rate;
        for (int t = 0; t < EQ10_NOFBANDS; t++)
            eq10_bsetup(rate, &eq->band[t], eq10_freq[t], EQ10_Q);
        eq->detect = 0;
        eq->detectdecay = pow(0.001, 1.0 / (rate * EQ10_TRIM_RELEASE));
    }
}

void eq10_setgain(eq10_t *eq, int eqs, int bandnr, double gain_dB) {
    double realgain = eq10_db2gain(gain_dB);
    for (int k = 0; k < eqs; k++)
        eq[k].band[bandnr].gain = realgain;
}

void eq10_processf(eq10_t *eq, float *buf, float *outbuf, int sz, int idx, int step) {
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
            double y0 = (in[0] - x2) * a0 + y1 * b1 + y2 * b2 + 1e-30;
            x2 = x1; x1 = in[0]; y2 = y1; y1 = y0;
            out[0] = (float)(y0 + in[0]);
        }
        in = outbuf;
        eq->band[k].x1 = x1; eq->band[k].x2 = x2;
        eq->band[k].y1 = y1; eq->band[k].y2 = y2;
    }

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
            detect += 1e-30;
        }
        eq->detect = detect;
    }
}
