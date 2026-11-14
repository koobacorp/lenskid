// LENSKID looks engine: colour grades (shared with the .cube LUT pack) and the spatial
// effects (grain, glow, warp, damage). Pure C++17, no host dependencies, deterministic.
// Copyright (c) 2026 koobacorp. All rights reserved.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include "font.h"

namespace lk {

enum Look { BOKETTO, DREAM, DASHCAM, SUPER8, MELT, INFRARED, THERMAL, NUM_LOOKS };
static const char* const kLookNames[NUM_LOOKS] = {"Boketto Moody Vintage", "Overexposed Dream", "Dashcam Junk", "Super 8",
                                                  "Melt Signal", "Infrared Ghost", "Thermal Heart"};
static const char* const kLookFiles[NUM_LOOKS] = {"LENSKID_01_Boketto_Moody_Vintage", "LENSKID_02_Overexposed_Dream", "LENSKID_03_Dashcam_Junk",
                                                  "LENSKID_04_Super_8", "LENSKID_05_Melt_Signal", "LENSKID_06_Infrared_Ghost", "LENSKID_07_Thermal_Heart"};

// Every slider is 0..1 where 0.5 is the look's designed amount (0 = off, 1 = double).
struct Params {
    int look = BOKETTO;
    float amount = 1, grain = .5f, glow = .5f, wobble = .5f, damage = .5f, real = 0, vignette = .5f;
    bool frameHold = true, lightLeaks = false, dateStamp = true, beatOn = false;
    float bpm = 120, beatOffset = 0;
    int seed = 0;
    std::string dateText = "2026.11.14  21:07";
};

struct Buf {  // float RGBA, row-major, y up (OFX order)
    int w = 0, h = 0;
    std::vector<float> p;
    void alloc(int W, int H) { w = W; h = H; p.assign((size_t)W * H * 4, 0.f); }
    float* px(int x, int y) { return &p[((size_t)y * w + x) * 4]; }
    const float* px(int x, int y) const { return &p[((size_t)y * w + x) * 4]; }
};

template <class F> inline void parallelRows(int h, F fn) {
    unsigned n = std::thread::hardware_concurrency();
    if (n < 1) n = 1;
    if (n > 16) n = 16;
    if (h < 64 || n == 1) { fn(0, h); return; }
    std::vector<std::thread> ts;
    int chunk = (h + (int)n - 1) / (int)n;
    for (unsigned i = 0; i < n; i++) {
        int a = (int)i * chunk, b = std::min(h, a + chunk);
        if (a >= b) break;
        ts.emplace_back([=] { fn(a, b); });
    }
    for (auto& t : ts) t.join();
}

// ---------- small maths ----------
inline float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : (x == x ? x : 0); }
inline float mixf(float a, float b, float t) { return a + (b - a) * t; }
inline float smooth(float e0, float e1, float x) { float t = clamp01((x - e0) / (e1 - e0)); return t * t * (3 - 2 * t); }
inline float luma(float r, float g, float b) { return 0.2126f * r + 0.7152f * g + 0.0722f * b; }
inline uint32_t hash3(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}
inline float rnd(uint32_t a, uint32_t b, uint32_t c) { return (hash3(a, b, c) >> 8) * (1.f / 16777216.f); }
inline float sCurve(float x, float k) { x = clamp01(x); float a = std::pow(x, k), b = std::pow(1 - x, k); return a / (a + b); }
// hue distance window, h and c in 0..1 (wraps), returns 1 at centre, 0 beyond half-width
inline float hueWin(float h, float c, float hw) { float d = std::fabs(h - c); d = std::min(d, 1 - d); return 1 - smooth(0.f, hw, d); }
inline void rgb2hsv(float r, float g, float b, float& h, float& s, float& v) {
    float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b)), d = mx - mn;
    v = mx; s = mx > 1e-6f ? d / mx : 0; h = 0;
    if (d > 1e-6f) {
        if (mx == r) h = (g - b) / d; else if (mx == g) h = 2 + (b - r) / d; else h = 4 + (r - g) / d;
        h /= 6; if (h < 0) h += 1;
    }
}
inline void hsv2rgb(float h, float s, float v, float& r, float& g, float& b) {
    h = h - std::floor(h); s = clamp01(s);
    float f = h * 6; int i = (int)f; f -= i;
    float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    switch (i % 6) { case 0: r = v; g = t; b = p; break; case 1: r = q; g = v; b = p; break; case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break; case 4: r = t; g = p; b = v; break; default: r = v; g = p; b = q; }
}
inline void saturate(float& r, float& g, float& b, float s) { float y = luma(r, g, b); r = y + (r - y) * s; g = y + (g - y) * s; b = y + (b - y) * s; }
inline float skinW(float h, float s, float v) { return hueWin(h, 0.065f, 0.1f) * smooth(0.08f, 0.35f, s) * (1 - smooth(0.7f, 1.0f, s)) * smooth(0.1f, 0.4f, v); }
inline float greenW(float h, float s) { return hueWin(h, 0.30f, 0.2f) * smooth(0.08f, 0.4f, s); }
inline float skyW(float r, float g, float b) {
    float h, s, v; rgb2hsv(r, g, b, h, s, v);
    return hueWin(h, 0.58f, 0.17f) * smooth(0.08f, 0.45f, s) * smooth(0.25f, 0.65f, v);
}

// ---------- colour grades (per pixel; Rec.709 display values in, out) ----------
// `dmg` is the Distortion/damage slider (0..1, 0.5 = designed); some looks use it for colour.
inline void grade(int look, float& r, float& g, float& b, float dmg = 0.5f) {
    r = clamp01(r); g = clamp01(g); b = clamp01(b);
    float h, s, v, y = luma(r, g, b);
    float q = dmg * 2;
    switch (look) {
    case BOKETTO: {  // teal-grey crushed shadows, muted greens, faded blacks, cool skin with a hint of warmth
        rgb2hsv(r, g, b, h, s, v);
        float gw = greenW(h, s), sk = skinW(h, s, v);
        h += 0.05f * gw;                         // greens lean teal/olive-grey
        s *= (1 - 0.6f * gw) * mixf(0.72f, 0.9f, sk);
        v *= 1 - 0.12f * gw;
        hsv2rgb(h, s, v, r, g, b);
        r = std::pow(sCurve(r, 1.5f), 1.12f); g = std::pow(sCurve(g, 1.5f), 1.12f); b = std::pow(sCurve(b, 1.5f), 1.12f);
        y = luma(r, g, b);
        float sh = (1 - y) * (1 - y), hl = y * y;
        r += -0.055f * sh + 0.025f * hl + 0.018f * sk;
        g += 0.010f * sh + 0.012f * hl + 0.004f * sk;
        b += 0.042f * sh - 0.020f * hl - 0.006f * sk;
        float lift = 0.045f * q;                 // faded, slightly teal blacks
        r = 0.9f * lift + r * (1 - lift - 0.03f * q); g = 1.0f * lift + g * (1 - lift - 0.03f * q); b = 1.08f * lift + b * (1 - lift - 0.03f * q);
        break; }
    case DREAM: {  // blown, lifted, warm-gold highlights, deep glowing greens
        float stops = 0.85f * q;
        auto expo = [&](float x) { float lin = std::pow(clamp01(x), 2.2f) * std::pow(2.f, stops); float t = 1 - std::exp(-2.2f * lin); return std::pow(t / (1 - std::exp(-2.2f)), 1 / 2.2f); };
        r = expo(r); g = expo(g); b = expo(b);
        r = std::pow(clamp01(r), 0.92f); g = std::pow(clamp01(g), 0.92f); b = std::pow(clamp01(b), 0.92f);
        rgb2hsv(r, g, b, h, s, v);
        float gw = greenW(h, s), sk = skinW(h, s, v);
        h = mixf(h, 0.37f, 0.4f * gw); s = std::min(1.f, s * (1 + 0.7f * gw) + 0.08f * gw); v *= 1 - 0.24f * gw;
        s *= 1 + 0.1f * sk;
        hsv2rgb(h, s, v, r, g, b);
        y = luma(r, g, b);
        float hl = smooth(0.5f, 1.0f, y);
        r += 0.06f * hl + 0.03f * sk; g += 0.035f * hl + 0.01f * sk; b -= 0.08f * hl;
        r = 0.045f + r * 0.955f; g = 0.04f + g * 0.96f; b = 0.03f + b * 0.97f;
        break; }
    case DASHCAM: {  // cheap sensor: purple-magenta cast, hard contrast, clipped highlights
        rgb2hsv(r, g, b, h, s, v);
        s *= 1.12f - 0.35f * greenW(h, s);
        hsv2rgb(h, s, v, r, g, b);
        r = sCurve(r, 1.5f); g = sCurve(g, 1.5f); b = sCurve(b, 1.5f);
        y = luma(r, g, b);
        float sh = 1 - y;
        r = r * 1.03f + 0.045f * sh + 0.02f + 0.02f * y; g = g * 0.9f - 0.01f * sh - 0.015f * y; b = b * 1.02f + 0.075f * sh + 0.025f + 0.035f * y;
        r = std::min(1.f, r * 1.06f); g = std::min(1.f, g * 1.06f); b = std::min(1.f, b * 1.06f);
        break; }
    case SUPER8: {  // Kodachrome-ish: warm, rich reds and yellows, deep blues, punchy contrast
        rgb2hsv(r, g, b, h, s, v);
        float red = hueWin(h, 0.0f, 0.07f), yel = hueWin(h, 0.12f, 0.07f), blu = hueWin(h, 0.6f, 0.1f), gw = greenW(h, s);
        h += -0.03f * gw + 0.02f * blu * -1.f;
        s *= 1.22f + 0.15f * red + 0.1f * yel;
        v *= 1 - 0.1f * blu;
        hsv2rgb(h, s, v, r, g, b);
        r = sCurve(r, 1.3f); g = sCurve(g, 1.3f); b = sCurve(b, 1.3f);
        y = luma(r, g, b);
        float sh = (1 - y) * (1 - y);
        r = r * 1.05f + 0.012f + 0.02f * sh; g = g * 1.0f + 0.006f; b = b * 0.86f + 0.01f * sh;
        r = 0.03f + r * 0.97f; g = 0.022f + g * 0.97f; b = 0.02f + b * 0.97f;
        break; }
    case MELT: {  // over-cooked signal: cyan-blue shadows, magenta highlights, hot saturation
        rgb2hsv(r, g, b, h, s, v);
        h += 0.03f; s = std::min(1.f, s * 1.55f + 0.05f);
        hsv2rgb(h, s, v, r, g, b);
        r = sCurve(r, 1.4f); g = sCurve(g, 1.4f); b = sCurve(b, 1.4f);
        y = luma(r, g, b);
        float sh = (1 - y) * (1 - y), hl = y * y;
        r += -0.05f * sh + 0.10f * hl; g += 0.025f * sh - 0.04f * hl; b += 0.10f * sh + 0.06f * hl;
        break; }
    case INFRARED: {  // fake infrared: pink-white foliage, black skies, glowing skin
        rgb2hsv(r, g, b, h, s, v);
        float gw = greenW(h, s) * smooth(0.05f, 0.4f, v), sk = skinW(h, s, v), sky = skyW(r, g, b);
        float ir = 0.45f * r + 0.45f * g + 0.1f * b;
        ir = mixf(ir, 0.55f + 0.5f * v, gw);            // chlorophyll glows
        ir = mixf(ir, std::min(1.f, ir * 1.25f + 0.12f), sk);  // skin glows
        ir *= 1 - 0.85f * sky;                           // skies go black
        ir *= 1 - 0.5f * hueWin(h, 0.62f, 0.17f) * smooth(0.08f, 0.45f, s);  // blue things darken
        ir = sCurve(std::min(1.f, ir), 1.25f);
        float pr = 1.0f, pg = mixf(1.0f, 0.70f, gw), pb = mixf(1.0f, 0.86f, gw);  // pink tint on foliage
        float shade = 1 - ir;
        r = ir * pr - 0.015f * shade; g = ir * pg - 0.01f * shade; b = ir * pb + 0.035f * shade * shade;
        break; }
    case THERMAL: {  // heat palette from brightness (skin and warm colours run hotter, sky colder)
        rgb2hsv(r, g, b, h, s, v);
        float heat = 0.62f * y + 0.2f * std::max(0.f, r - b) + 0.38f * skinW(h, s, v) - 0.32f * greenW(h, s) - 0.25f * skyW(r, g, b) + 0.06f;
        heat = clamp01(heat * 1.08f);
        float levels = 96.f / (3 * q * q + 1);
        heat = std::floor(heat * levels + 0.5f) / levels;
        static const float st[7][4] = {{0.f, .02f, .0f, .10f}, {.2f, .10f, .05f, .62f}, {.4f, .58f, .05f, .66f}, {.6f, .93f, .10f, .18f},
                                       {.78f, 1.f, .55f, .05f}, {.92f, 1.f, .90f, .30f}, {1.f, 1.f, 1.f, .92f}};
        int i = 0; while (i < 5 && heat > st[i + 1][0]) i++;
        float t = clamp01((heat - st[i][0]) / (st[i + 1][0] - st[i][0]));
        r = mixf(st[i][1], st[i + 1][1], t); g = mixf(st[i][2], st[i + 1][2], t); b = mixf(st[i][3], st[i + 1][3], t);
        break; }
    }
    r = clamp01(r); g = clamp01(g); b = clamp01(b);
}

// ---------- sampling / blur helpers ----------
inline void sampleBil(const Buf& b, float x, float y, float* o, bool wrapX = false) {
    // x, y in pixel coordinates (centres at +0.5)
    x -= 0.5f; y -= 0.5f;
    if (wrapX) { x = std::fmod(x, (float)b.w); if (x < 0) x += b.w; }
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y); float fx = x - x0, fy = y - y0;
    auto cx = [&](int v) { if (wrapX) { v %= b.w; if (v < 0) v += b.w; return v; } return v < 0 ? 0 : v >= b.w ? b.w - 1 : v; };
    auto cy = [&](int v) { return v < 0 ? 0 : v >= b.h ? b.h - 1 : v; };
    const float *a = b.px(cx(x0), cy(y0)), *c = b.px(cx(x0 + 1), cy(y0)), *d = b.px(cx(x0), cy(y0 + 1)), *e = b.px(cx(x0 + 1), cy(y0 + 1));
    for (int k = 0; k < 4; k++) o[k] = (a[k] * (1 - fx) + c[k] * fx) * (1 - fy) + (d[k] * (1 - fx) + e[k] * fx) * fy;
}
// box-average downscale by integer factor f (edge blocks average what they have)
inline Buf down(const Buf& in, int f) {
    Buf o; int W = std::max(1, (in.w + f - 1) / f), H = std::max(1, (in.h + f - 1) / f); o.alloc(W, H);
    parallelRows(H, [&](int r0, int r1) {
        for (int y = r0; y < r1; y++) for (int x = 0; x < W; x++) {
            float acc[4] = {0, 0, 0, 0}; int n = 0;
            for (int yy = y * f; yy < std::min(in.h, y * f + f); yy++) for (int xx = x * f; xx < std::min(in.w, x * f + f); xx++) { const float* p = in.px(xx, yy); for (int k = 0; k < 4; k++) acc[k] += p[k]; n++; }
            float* q = o.px(x, y); for (int k = 0; k < 4; k++) q[k] = n ? acc[k] / n : 0;
        }
    });
    return o;
}
// separable box blur, `passes` times (3 passes ~ gaussian), radius in pixels of this buffer
inline void boxBlur(Buf& b, int rad, int passes = 3) {
    if (rad < 1 || b.w < 1 || b.h < 1) return;
    std::vector<float> tmp(b.p.size());
    for (int pass = 0; pass < passes; pass++) {
        parallelRows(b.h, [&](int r0, int r1) {  // horizontal
            std::vector<float> acc(4);
            for (int y = r0; y < r1; y++) {
                for (int k = 0; k < 4; k++) acc[k] = 0;
                for (int i = -rad; i <= rad; i++) { const float* p = b.px(std::min(b.w - 1, std::max(0, i)), y); for (int k = 0; k < 4; k++) acc[k] += p[k]; }
                for (int x = 0; x < b.w; x++) {
                    float* o = &tmp[((size_t)y * b.w + x) * 4]; for (int k = 0; k < 4; k++) o[k] = acc[k] / (2 * rad + 1);
                    const float* add = b.px(std::min(b.w - 1, x + rad + 1), y); const float* sub = b.px(std::max(0, x - rad), y);
                    for (int k = 0; k < 4; k++) acc[k] += add[k] - sub[k];
                }
            }
        });
        // vertical, column chunks
        int W = b.w, H = b.h;
        parallelRows(W, [&](int c0, int c1) {
            std::vector<float> acc(4);
            for (int x = c0; x < c1; x++) {
                auto T = [&](int y) { return &tmp[((size_t)std::min(H - 1, std::max(0, y)) * W + x) * 4]; };
                for (int k = 0; k < 4; k++) acc[k] = 0;
                for (int i = -rad; i <= rad; i++) { const float* p = T(i); for (int k = 0; k < 4; k++) acc[k] += p[k]; }
                for (int y = 0; y < H; y++) {
                    float* o = b.px(x, y); for (int k = 0; k < 4; k++) o[k] = acc[k] / (2 * rad + 1);
                    const float* add = T(y + rad + 1); const float* sub = T(y - rad);
                    for (int k = 0; k < 4; k++) acc[k] += add[k] - sub[k];
                }
            }
        });
    }
}
// blurred copy at full-res coordinates: downscale by f, blur with radius rad (full-res px), sample back bilinear
struct Blurred {
    Buf low; int f = 1;
    void make(const Buf& in, float radFull) {
        f = std::max(1, std::min(16, (int)std::floor(radFull / 3)));
        low = f > 1 ? down(in, f) : in;
        boxBlur(low, std::max(1, (int)std::lround(radFull / f / 1.7f)));
    }
    void at(int x, int y, float* o) const { sampleBil(low, (x + 0.5f) / f, (y + 0.5f) / f, o); }
};

// value noise in -1..1 with cell size `cell` px, per frame
inline float vnoise(float x, float y, float cell, uint32_t fr, uint32_t ch) {
    float gx = x / cell, gy = y / cell; int x0 = (int)std::floor(gx), y0 = (int)std::floor(gy); float fx = gx - x0, fy = gy - y0;
    auto n = [&](int a, int b) { return rnd((uint32_t)a * 2 + ch, (uint32_t)b, fr) + rnd((uint32_t)a, (uint32_t)b * 3 + ch, fr ^ 0x5bd1e995u) - 1.f; };
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    return mixf(mixf(n(x0, y0), n(x0 + 1, y0), fx), mixf(n(x0, y0 + 1), n(x0 + 1, y0 + 1), fx), fy) * 1.6f;
}

// ---------- time helpers ----------
// 18 fps frame hold for Super 8: which source frame to show at frame t
inline double holdTime(const Params& p, double t, double fps) {
    if (p.look != SUPER8 || !p.frameHold || p.amount <= 0 || fps <= 18.5) return t;
    double sec = t / fps; double held = std::floor(sec * 18.0 + 1e-9) / 18.0;
    return std::min(t, std::ceil(held * fps - 1e-6));
}
inline float beatEnv(const Params& p, double t, double fps) {
    if (!p.beatOn || p.bpm <= 0 || fps <= 0) return 1.f;
    double b = t / fps * p.bpm / 60.0 - p.beatOffset; double ph = b - std::floor(b);
    return 0.2f + 1.6f * (float)std::exp(-ph * 5.0);
}

// ---------- the full look ----------
// src and dst are the same size; t = frame number, fps = clip frame rate. Output values are 0..1, alpha copied.
inline void render(const Buf& src, Buf& dst, const Params& P, double t, double fps) {
    const int w = src.w, h = src.h;
    dst.alloc(w, h);
    if (w <= 0 || h <= 0) return;
    const int L = std::max(0, std::min(NUM_LOOKS - 1, P.look));
    const float k = h / 1080.f, aspect = (float)w / h;
    const float gr = clamp01(P.grain) * 2, gl = clamp01(P.glow) * 2, wb = clamp01(P.wobble) * 2, dm = clamp01(P.damage) * 2, vg = clamp01(P.vignette) * 2;
    const float real = clamp01(P.real), amount = clamp01(P.amount);
    const uint32_t fr = (uint32_t)(int64_t)std::floor(t + 0.5), seed = (uint32_t)P.seed * 7919u + 17u;
    const float sec = (float)(t / std::max(1.0, fps));
    const float beat = (L == MELT) ? beatEnv(P, t, fps) : 1.f;

    // clamp the input once (looks are display-referred)
    Buf in; in.alloc(w, h);
    parallelRows(h, [&](int r0, int r1) { for (size_t i = (size_t)r0 * w * 4; i < (size_t)r1 * w * 4; i++) in.p[i] = clamp01(src.p[i]); });

    // 1. geometry / signal damage -> base (still real colour)
    Buf base; base.alloc(w, h);
    // Super 8 gate weave
    float wx = 0, wy = 0, zoom = 1;
    if (L == SUPER8) {
        float a = 0.0035f * h * wb;
        wx = a * (std::sin(sec * 7.1f + seed) * 0.6f + (rnd(fr, seed, 1) - 0.5f) * 0.8f);
        wy = a * 0.8f * (std::sin(sec * 5.3f + 1.7f + seed) * 0.6f + (rnd(fr, seed, 2) - 0.5f) * 0.8f);
        zoom = 1 + 0.012f * wb;
    }
    float fish = (L == DASHCAM) ? 0.32f * wb : 0.f;
    float rmax2 = (aspect * aspect + 1) * 0.25f;
    // Melt: row bands that slide sideways, and RGB tearing
    std::vector<float> rowShift, rowTear;
    if (L == MELT) {
        rowShift.assign(h, 0.f); rowTear.assign(h, 0.f);
        uint32_t grp = fr / 2;  // bands change every 2 frames
        int y = 0, band = 0;
        while (y < h) {
            int bh = std::max(1, (int)std::lround(k * (4 + 60 * rnd(band, grp, seed + 3))));
            float on = rnd(band, grp, seed + 4) < 0.38f ? 1.f : 0.f;
            float sft = on * (rnd(band, grp, seed + 5) - 0.5f) * 0.22f * w * wb * beat;
            float tear = (0.35f + 0.65f * rnd(band, grp, seed + 6)) * 0.018f * w * dm * beat;
            for (int yy = y; yy < std::min(h, y + bh); yy++) { rowShift[yy] = sft; rowTear[yy] = tear; }
            y += bh; band++;
        }
    }
    parallelRows(h, [&](int r0, int r1) {
        float o[4], tmp[4];
        for (int y = r0; y < r1; y++) for (int x = 0; x < w; x++) {
            float X = x + 0.5f, Y = y + 0.5f;
            float* d = base.px(x, y);
            if (L == SUPER8) {
                float cx = w * 0.5f, cy = h * 0.5f;
                sampleBil(in, cx + (X - cx) / zoom + wx, cy + (Y - cy) / zoom + wy, d);
            } else if (L == DASHCAM && fish > 0) {
                float px = (X / w - 0.5f) * aspect, py = Y / h - 0.5f, r2 = px * px + py * py;
                float m = (1 + fish * r2) / (1 + fish * rmax2);
                sampleBil(in, (px * m / aspect + 0.5f) * w, (py * m + 0.5f) * h, d);
            } else if (L == MELT) {
                float sx = X + rowShift[y];
                sampleBil(in, sx + rowTear[y], Y, tmp, true); d[0] = tmp[0];
                sampleBil(in, sx, Y, tmp, true); d[1] = tmp[1]; d[3] = tmp[3];
                sampleBil(in, sx - rowTear[y], Y, tmp, true); d[2] = tmp[2];
            } else if (L == THERMAL && wb > 0) {
                float dx = (std::sin(Y / h * 37.f + sec * 2.1f) * 0.7f + std::sin(Y / h * 91.f - sec * 3.3f) * 0.3f) * 0.0025f * w * wb;
                float dy = std::sin(X / w * 29.f + sec * 1.7f) * 0.0015f * h * wb;
                sampleBil(in, X + dx, Y + dy, d);
            } else if (L == BOKETTO && wb > 0) {  // vintage lens: soft radial colour fringe
                float cx = w * 0.5f, cy = h * 0.5f, ca = 0.0022f * wb;
                sampleBil(in, cx + (X - cx) * (1 - ca), cy + (Y - cy) * (1 - ca), tmp); d[0] = tmp[0];
                const float* g = in.px(x, y); d[1] = g[1]; d[3] = g[3];
                sampleBil(in, cx + (X - cx) * (1 + ca), cy + (Y - cy) * (1 + ca), tmp); d[2] = tmp[2];
            } else {
                const float* s = in.px(x, y); for (int c = 0; c < 4; c++) d[c] = s[c];
            }
            (void)o;
        }
    });
    // Dashcam / Thermal: low resolution sensor, blocky compression, over-sharpening
    if ((L == DASHCAM || L == THERMAL) && dm > 0) {
        int bs = std::max(1, (int)std::lround(1 + (L == DASHCAM ? 2.6f : 2.0f) * dm * k));
        {
            Buf lo = bs > 1 ? down(base, bs) : base;
            if (L == DASHCAM) {
                // 8x8 "macroblocks": flatten low-detail blocks toward their mean, 2x2 chroma
                Buf c2 = down(lo, 2);
                int B = 8; float flat = std::min(1.f, 0.55f * dm);
                for (int by = 0; by < lo.h; by += B) for (int bx = 0; bx < lo.w; bx += B) {
                    float m[3] = {0, 0, 0}, v = 0; int n = 0;
                    for (int y = by; y < std::min(lo.h, by + B); y++) for (int x = bx; x < std::min(lo.w, bx + B); x++) { const float* p = lo.px(x, y); for (int c = 0; c < 3; c++) m[c] += p[c]; n++; }
                    for (int c = 0; c < 3; c++) m[c] /= n;
                    for (int y = by; y < std::min(lo.h, by + B); y++) for (int x = bx; x < std::min(lo.w, bx + B); x++) { const float* p = lo.px(x, y); float d0 = luma(p[0], p[1], p[2]) - luma(m[0], m[1], m[2]); v += d0 * d0; }
                    v /= n;
                    float f = flat * (1 - smooth(0.001f, 0.012f, v));
                    for (int y = by; y < std::min(lo.h, by + B); y++) for (int x = bx; x < std::min(lo.w, bx + B); x++) {
                        float* p = lo.px(x, y); const float* cc = c2.px(std::min(c2.w - 1, x / 2), std::min(c2.h - 1, y / 2));
                        float Y0 = luma(p[0], p[1], p[2]), Yc = luma(cc[0], cc[1], cc[2]);
                        for (int c = 0; c < 3; c++) { float chroma = mixf(p[c] - Y0, cc[c] - Yc, std::min(1.f, 0.8f * dm)); p[c] = Y0 + chroma; p[c] = mixf(p[c], m[c], f); }
                    }
                }
                // over-sharpen at sensor resolution (halos)
                Buf bl = lo; boxBlur(bl, 1, 2);
                float amt = 0.6f + 1.4f * dm;
                for (size_t i = 0; i < lo.p.size(); i++) if (i % 4 != 3) lo.p[i] = clamp01(lo.p[i] + amt * (lo.p[i] - bl.p[i]));
            }
            parallelRows(h, [&](int r0, int r1) {
                for (int y = r0; y < r1; y++) for (int x = 0; x < w; x++) {
                    const float* s = lo.px(std::min(lo.w - 1, x / bs), std::min(lo.h - 1, y / bs)); float* d = base.px(x, y);
                    for (int c = 0; c < 3; c++) d[c] = s[c];
                }
            });
        }
    }

    // 2. colour grade, blended back toward real colour
    Buf out; out.alloc(w, h);
    parallelRows(h, [&](int r0, int r1) {
        for (int y = r0; y < r1; y++) for (int x = 0; x < w; x++) {
            const float* b = base.px(x, y); float* o = out.px(x, y);
            float r = b[0], g = b[1], bb = b[2]; grade(L, r, g, bb, P.damage);
            o[0] = mixf(r, b[0], real); o[1] = mixf(g, b[1], real); o[2] = mixf(bb, b[2], real); o[3] = b[3];
        }
    });

    // 3. look-specific softness: Boketto edge softness, Dream mist diffusion
    if ((L == BOKETTO || L == DREAM) && wb > 0) {
        Blurred bl; bl.make(out, (L == DREAM ? 14.f : 5.f) * k * (0.5f + 0.5f * wb) + 1);
        parallelRows(h, [&](int r0, int r1) {
            float s[4];
            for (int y = r0; y < r1; y++) for (int x = 0; x < w; x++) {
                float* o = out.px(x, y); bl.at(x, y, s);
                float px = ((x + 0.5f) / w - 0.5f) * aspect, py = (y + 0.5f) / h - 0.5f, r2 = (px * px + py * py) / rmax2;
                float m;
                if (L == BOKETTO) m = std::min(1.f, r2 * 0.9f * wb);
                else { float yb = luma(s[0], s[1], s[2]); m = std::min(0.85f, (0.08f + 0.22f * yb) * wb); for (int c = 0; c < 3; c++) s[c] = std::max(s[c], o[c]); }
                for (int c = 0; c < 3; c++) o[c] = mixf(o[c], s[c], m);
            }
        });
    }
    // Melt: colour bleed smeared to the right along each row
    if (L == MELT && dm > 0) {
        float kk = std::min(0.97f, 1 - 1 / (1 + 6 * k * dm * beat));
        parallelRows(h, [&](int r0, int r1) {
            for (int y = r0; y < r1; y++) {
                float acc[3]; float* p0 = out.px(0, y); for (int c = 0; c < 3; c++) acc[c] = p0[c];
                for (int x = 0; x < w; x++) {
                    float* p = out.px(x, y); float Y0 = luma(p[0], p[1], p[2]);
                    for (int c = 0; c < 3; c++) acc[c] = mixf(p[c], acc[c], kk);
                    float Ya = luma(acc[0], acc[1], acc[2]);
                    for (int c = 0; c < 3; c++) p[c] = clamp01(Y0 + mixf(p[c] - Y0, acc[c] - Ya, 0.75f) * 1.15f);
                }
            }
        });
    }

    // 4. glow / halation (screen blend of blurred highlights)
    if (gl > 0) {
        struct Layer { float thr, rad, str, r, g, b; };
        Layer ls[2]; int nl = 1;
        switch (L) {
            case BOKETTO: ls[0] = {0.68f, 10, 0.28f, 1, .95f, .88f}; break;
            case DREAM: ls[0] = {0.55f, 22, 0.34f, 1, .84f, .52f}; ls[1] = {0.68f, 55, 0.26f, 1, .36f, .12f}; nl = 2; break;
            case DASHCAM: ls[0] = {0.78f, 7, 0.4f, 1, .9f, 1}; break;
            case SUPER8: ls[0] = {0.66f, 16, 0.45f, 1, .5f, .25f}; break;
            case MELT: ls[0] = {0.45f, 20, 0.55f, 1, .3f, .9f}; break;
            case INFRARED: ls[0] = {0.55f, 24, 0.7f, 1, .93f, .97f}; break;
            default: ls[0] = {0.55f, 16, 0.5f, 1, .62f, .22f}; break;
        }
        for (int li = 0; li < nl; li++) {
            const Layer& Ly = ls[li];
            Buf hi; hi.alloc(w, h);
            parallelRows(h, [&](int r0, int r1) {
                for (int y = r0; y < r1; y++) for (int x = 0; x < w; x++) {
                    const float* o = out.px(x, y); float* q = hi.px(x, y);
                    float m = smooth(Ly.thr, 1.0f, luma(o[0], o[1], o[2]));
                    q[0] = o[0] * m * Ly.r; q[1] = o[1] * m * Ly.g; q[2] = o[2] * m * Ly.b; q[3] = 0;
                }
            });
            Blurred bl; bl.make(hi, Ly.rad * k * (0.6f + 0.4f * gl) + 1);
            float s = Ly.str * gl;
            parallelRows(h, [&](int r0, int r1) {
                float g[4];
                for (int y = r0; y < r1; y++) for (int x = 0; x < w; x++) {
                    float* o = out.px(x, y); bl.at(x, y, g);
                    for (int c = 0; c < 3; c++) { float v = clamp01(g[c] * s * 2.2f); o[c] = 1 - (1 - clamp01(o[c])) * (1 - v); }
                }
            });
        }
    }

    // 5. Infrared ghost: faint offset double exposure
    if (L == INFRARED && dm > 0) {
        Buf cp = out;
        float dx = (0.012f + 0.02f * wb) * w, dy = -(0.004f + 0.008f * wb) * h, sc = 1 + 0.015f * wb, op = 0.32f * dm;
        parallelRows(h, [&](int r0, int r1) {
            float g[4];
            for (int y = r0; y < r1; y++) for (int x = 0; x < w; x++) {
                float cx = w * 0.5f, cy = h * 0.5f;
                sampleBil(cp, cx + (x + 0.5f - cx) / sc - dx, cy + (y + 0.5f - cy) / sc - dy, g);
                float* o = out.px(x, y);
                for (int c = 0; c < 3; c++) o[c] = 1 - (1 - o[c]) * (1 - g[c] * op);
            }
        });
    }

    // 6. Super 8: flicker, dust and scratches; light leaks (any look)
    if (L == SUPER8) {
        float fl = 1 + 0.07f * wb * (rnd(fr, seed, 9) - 0.5f) * 2;
        for (size_t i = 0; i < out.p.size(); i++) if (i % 4 != 3) out.p[i] *= fl;
        if (dm > 0) {
            int specks = (int)std::lround(28 * dm * std::max(0.3f, (float)w * h / (1920.f * 1080.f)));
            for (int i = 0; i < specks; i++) {
                float cx = rnd(fr, i, seed + 11) * w, cy = rnd(fr, i, seed + 12) * h;
                float rx = (0.8f + 3.5f * rnd(fr, i, seed + 13) * rnd(fr, i, seed + 14)) * std::max(1.0f, k), ry = rx * (0.4f + 1.2f * rnd(fr, i, seed + 15));
                bool white = rnd(fr, i, seed + 16) < 0.25f; float op = 0.55f + 0.4f * rnd(fr, i, seed + 17);
                for (int y = std::max(0, (int)(cy - ry - 2)); y <= std::min(h - 1, (int)(cy + ry + 2)); y++)
                    for (int x = std::max(0, (int)(cx - rx - 2)); x <= std::min(w - 1, (int)(cx + rx + 2)); x++) {
                        float ddx = (x + 0.5f - cx) / rx, ddy = (y + 0.5f - cy) / ry, e = ddx * ddx + ddy * ddy;
                        float m = op * (1 - smooth(0.6f, 1.2f, e)); if (m <= 0) continue;
                        float* o = out.px(x, y); for (int c = 0; c < 3; c++) o[c] = white ? mixf(o[c], 0.95f, m) : o[c] * (1 - m * 0.9f);
                    }
            }
            int ns = rnd(fr / 6, seed, 21) < 0.6f * std::min(1.f, dm) ? 1 + (rnd(fr / 6, seed, 22) < 0.3f) : 0;
            for (int i = 0; i < ns; i++) {
                float sx = (rnd(fr / 24, i, seed + 23) + 0.02f * (rnd(fr, i, seed + 24) - 0.5f)) * w, sw = std::max(0.7f, 1.2f * k);
                bool white = rnd(fr / 24, i, seed + 25) < 0.5f;
                for (int y = 0; y < h; y++) {
                    float m = 0.35f * (0.6f + 0.4f * rnd(y / std::max(1, (int)(8 * k)), i, fr)); 
                    for (int x = std::max(0, (int)(sx - sw - 1)); x <= std::min(w - 1, (int)(sx + sw + 1)); x++) {
                        float mm = m * (1 - smooth(0.3f, 1.f, std::fabs(x + 0.5f - sx) / sw)); float* o = out.px(x, y);
                        for (int c = 0; c < 3; c++) o[c] = white ? mixf(o[c], 1.f, mm) : o[c] * (1 - mm);
                    }
                }
            }
        }
    }
    if (P.lightLeaks) {
        float ph = sec * 0.35f + seed * 0.37f;
        float cx = 0.5f + 0.62f * std::sin(ph), cy = 0.55f + 0.3f * std::sin(ph * 1.7f + 1.3f);
        float pulse = 0.65f + 0.35f * std::sin(sec * 1.3f + seed);
        parallelRows(h, [&](int r0, int r1) {
            for (int y = r0; y < r1; y++) for (int x = 0; x < w; x++) {
                float u = ((x + 0.5f) / w - cx) * aspect / 0.55f, v = ((y + 0.5f) / h - cy) / 0.8f;
                float m = std::exp(-(u * u + v * v) * 1.6f) * 0.75f * pulse, m2 = std::exp(-(u * u + v * v) * 6.f) * 0.5f * pulse;
                float* o = out.px(x, y);
                float lr = m * 1.0f + m2, lg = m * 0.42f + m2 * 0.8f, lb = m * 0.12f + m2 * 0.4f;
                o[0] = 1 - (1 - clamp01(o[0])) * (1 - clamp01(lr)); o[1] = 1 - (1 - clamp01(o[1])) * (1 - clamp01(lg)); o[2] = 1 - (1 - clamp01(o[2])) * (1 - clamp01(lb));
            }
        });
    }

    // 7. vignette (Super 8 adds rounded dark corners of the film gate)
    {
        static const float vs[NUM_LOOKS] = {0.38f, 0.12f, 0.3f, 0.32f, 0.18f, 0.3f, 0.2f};
        float vstr = vs[L] * vg;
        if (vstr > 0) parallelRows(h, [&](int r0, int r1) {
            for (int y = r0; y < r1; y++) for (int x = 0; x < w; x++) {
                float px = ((x + 0.5f) / w - 0.5f) * 2, py = ((y + 0.5f) / h - 0.5f) * 2;
                float d2 = (px * px + py * py) * 0.5f;
                float m = vstr * smooth(0.15f, 1.0f, d2);
                if (L == SUPER8) {  // rounded-rectangle gate
                    float rr = 0.22f, qx = std::fabs(px) * aspect - (aspect - rr), qy = std::fabs(py) - (1 - rr);
                    float ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
                    float sd = std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.f) - rr;  // <0 inside
                    m = std::max(m, std::min(1.f, vg * 0.5f) * 1.8f * smooth(-0.1f, 0.02f, sd));
                }
                m = std::min(1.f, m);
                float* o = out.px(x, y); for (int c = 0; c < 3; c++) o[c] *= 1 - m;
            }
        });
    }

    // 8. grain (luma-weighted, per frame; Dashcam gets chroma noise)
    if (gr > 0) {
        static const float ga[NUM_LOOKS] = {0.035f, 0.03f, 0.045f, 0.085f, 0.05f, 0.06f, 0.035f};
        static const float gs[NUM_LOOKS] = {1.1f, 1.7f, 1.3f, 2.1f, 1.2f, 1.6f, 1.4f};
        float amt = ga[L] * gr, cell = std::max(0.75f, gs[L] * k);
        bool chroma = (L == DASHCAM || L == MELT);
        uint32_t gf = fr * 31u + seed;
        parallelRows(h, [&](int r0, int r1) {
            for (int y = r0; y < r1; y++) for (int x = 0; x < w; x++) {
                float* o = out.px(x, y); float Y = luma(o[0], o[1], o[2]);
                float wgt = 0.35f + 2.6f * Y * (1 - Y);
                float n = vnoise((float)x, (float)y, cell, gf, 0) * amt * wgt;
                if (chroma) for (int c = 0; c < 3; c++) o[c] += n + vnoise((float)x, (float)y, cell * 1.5f, gf, c + 1) * amt * 0.6f * wgt;
                else for (int c = 0; c < 3; c++) o[c] += n;
            }
        });
    }

    // 9. Dashcam date stamp
    if (L == DASHCAM && P.dateStamp && !P.dateText.empty()) {
        int cs = std::max(1, (int)std::lround(3.2f * k));  // font pixel size
        int n = (int)P.dateText.size(), tw = n * kFontW * cs, th = kFontH * cs;
        int x0 = w - tw - (int)(0.04f * w), yTop = (int)(0.06f * h) + th;  // y-up: baseline near bottom
        for (int s = 1; s >= 0; s--)  // all shadows first, then the orange characters
        for (int i = 0; i < n; i++) {
            unsigned char ch = (unsigned char)P.dateText[i]; if (ch < 32 || ch > 126) ch = '?';
            const unsigned char* g = kFont[ch - 32];
            for (int ry = 0; ry < kFontH; ry++) for (int rx = 0; rx < kFontW; rx++) {
                if (!(g[ry] >> rx & 1)) continue;
                for (int sy = 0; sy < cs; sy++) for (int sx = 0; sx < cs; sx++) {
                    int XX = x0 + (i * kFontW + rx) * cs + sx + s * cs, YY = yTop - ry * cs - sy - s * cs;
                    if (XX < 0 || XX >= w || YY < 0 || YY >= h) continue;
                    float* o = out.px(XX, YY);
                    if (s) { for (int c = 0; c < 3; c++) o[c] *= 0.35f; }
                    else { o[0] = 1.f; o[1] = 0.62f; o[2] = 0.12f; }
                }
            }
        }
    }

    // 10. Amount mix with the untouched source, clamp, keep alpha
    parallelRows(h, [&](int r0, int r1) {
        for (int y = r0; y < r1; y++) for (int x = 0; x < w; x++) {
            const float* s = in.px(x, y); const float* o = out.px(x, y); float* d = dst.px(x, y);
            for (int c = 0; c < 3; c++) d[c] = clamp01(mixf(s[c], clamp01(o[c]), amount));
            d[3] = src.px(x, y)[3];
        }
    });
}

// .cube text for one look (33-point, Rec.709 in/out, colour only at designed settings)
inline std::string cubeText(int look, int N = 33) {
    std::string s = "TITLE \"" + std::string(kLookNames[look]) + " (LENSKID)\"\n# LENSKID by koobacorp. Rec.709 display in -> Rec.709 display out. Colour only:\n# grain, glow, wobble and damage live in the LENSKID plugin.\nLUT_3D_SIZE " + std::to_string(N) + "\nDOMAIN_MIN 0.0 0.0 0.0\nDOMAIN_MAX 1.0 1.0 1.0\n";
    char line[96];
    for (int b = 0; b < N; b++) for (int g = 0; g < N; g++) for (int r = 0; r < N; r++) {  // red changes fastest
        float R = r / (float)(N - 1), G = g / (float)(N - 1), B = b / (float)(N - 1);
        grade(look, R, G, B, 0.5f);
        snprintf(line, sizeof line, "%.6f %.6f %.6f\n", R, G, B); s += line;
    }
    return s;
}

}  // namespace lk
