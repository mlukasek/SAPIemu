#include "plant/thermocouple.h"

#include <math.h>
#include <string.h>

// NIST ITS-90 reference functions (NIST Monograph 175): E = sum c[i] t^i (mV)
static double poly(const double* c, int n, double t)
{
    double e = 0;
    for (int i = n - 1; i >= 0; i--) e = e * t + c[i];
    return e;
}

static double emf_k(double t)
{
    static const double neg[11] = { 0, 0.394501280250e-1, 0.236223735980e-4, -0.328589067840e-6, -0.499048287770e-8,
                                    -0.675090591730e-10, -0.574103274280e-12, -0.310888728940e-14, -0.104516093650e-16,
                                    -0.198892668780e-19, -0.163226974860e-22 };
    static const double pos[10] = { -0.176004136860e-1, 0.389212049750e-1, 0.185587700320e-4, -0.994575928740e-7,
                                    0.318409457190e-9, -0.560728448890e-12, 0.560750590590e-15, -0.320207200030e-18,
                                    0.971511471520e-22, -0.121047212750e-25 };
    if (t < 0) return poly(neg, 11, t);
    double a = t - 0.126968600000e3;
    return poly(pos, 10, t) + 0.118597600000 * exp(-0.118343200000e-3 * a * a);
}

static double emf_s(double t)
{
    static const double c1[9] = { 0, 0.540313308631e-2, 0.125934289740e-4, -0.232477968689e-7, 0.322028823036e-10,
                                  -0.331465196389e-13, 0.255744251786e-16, -0.125068871393e-19, 0.271443176145e-23 };
    static const double c2[5] = { 0.132900444085e1, 0.334509311344e-2, 0.654805192818e-5, -0.164856259209e-8,
                                  0.129989605174e-13 };
    static const double c3[5] = { 0.146628232636e3, -0.258430516752, 0.163693574641e-3, -0.330439046987e-7,
                                  -0.943223690612e-14 };
    if (t < 1064.18) return poly(c1, 9, t);
    if (t < 1664.5) return poly(c2, 5, t);
    return poly(c3, 5, t);
}

static double emf_b(double t)
{
    static const double c1[7] = { 0, -0.246508183460e-3, 0.590404211710e-5, -0.132579316360e-8, 0.156682919010e-11,
                                  -0.169445292400e-14, 0.629903470940e-18 };
    static const double c2[9] = { -0.389381686210e1, 0.285717474700e-1, -0.848851047850e-4, 0.157852801640e-6,
                                  -0.168353448640e-9, 0.111097940130e-12, -0.445154310330e-16, 0.989756408210e-20,
                                  -0.937913302890e-24 };
    if (t < 630.615) return poly(c1, 7, t);
    return poly(c2, 9, t);
}

// Tabulated types every 100 degC from -200 degC: monotone cubic interpolation (Fritsch-Carlson)
static double table(const double* e, int n, double t)
{
    double x = (t + 200.0) / 100.0;
    if (x <= 0) return e[0] + (e[1] - e[0]) * x;
    if (x >= n - 1) return e[n - 1] + (e[n - 1] - e[n - 2]) * (x - (n - 1));
    int i = (int)x;
    double u = x - i;
    auto slope = [&](int k) {
        if (k <= 0) return e[1] - e[0];
        if (k >= n - 1) return e[n - 1] - e[n - 2];
        double d0 = e[k] - e[k - 1], d1 = e[k + 1] - e[k];
        return d0 * d1 <= 0 ? 0.0 : 2.0 * d0 * d1 / (d0 + d1);
    };
    double m0 = slope(i), m1 = slope(i + 1), y0 = e[i], y1 = e[i + 1];
    double u2 = u * u, u3 = u2 * u;
    return (2 * u3 - 3 * u2 + 1) * y0 + (u3 - 2 * u2 + u) * m0 + (-2 * u3 + 3 * u2) * y1 + (u3 - u2) * m1;
}

static double emf_feko(double t)   // DIN 43710 type L (Fe-CuNi), -200 ... 900 degC
{
    static const double e[12] = { -8.15, -4.75, 0.0, 5.37, 10.95, 16.56, 22.16, 27.85, 33.67, 39.72, 46.22, 53.14 };
    return table(e, 12, t);
}

static double emf_chk(double t)    // chromel-copel (GOST type L, XK), -200 ... 800 degC
{
    static const double e[11] = { -9.488, -5.641, 0.0, 6.862, 14.560, 22.843, 31.492, 40.299, 49.108, 57.859, 66.466 };
    return table(e, 11, t);
}

double tc_emf_mv(int type, double c)
{
    switch (type) {
    case TC_FEKO: return emf_feko(c);
    case TC_CHA: return emf_k(c);
    case TC_B: return emf_b(c);
    case TC_S: return emf_s(c);
    case TC_CHK: return emf_chk(c);
    default: return 0;
    }
}

double tc_temp_c(int type, double mv)
{
    double lo = -200, hi = 1900;
    if (type == TC_B) lo = 50;   // B is not monotone below 50 degC
    for (int i = 0; i < 60; i++) {
        double mid = 0.5 * (lo + hi);
        if (tc_emf_mv(type, mid) < mv) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

static const char* const k_names[TC_COUNT] = { "Fe-Ko", "ch-a", "PtRh30-PtRh6", "PtRh10-Pt", "ch-k" };

int tc_parse(const std::string& name)
{
    for (int i = 0; i < TC_COUNT; i++)
        if (name == k_names[i]) return i;
    return -1;
}

const char* tc_name(int type)
{
    return type >= 0 && type < TC_COUNT ? k_names[type] : "?";
}
