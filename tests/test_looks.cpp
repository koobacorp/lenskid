// LENSKID engine tests: every look, every slider, ranges, determinism, odd sizes, LUTs.
#include <cstdio>
#include <cmath>
#include <chrono>
#include <cstring>
#include <sstream>
#include "looks.h"
using namespace lk;
static int fails = 0, checks = 0;
#define EXPECT(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// synthetic "photo": sky, foliage, skin, dark clothes, bright highlight, gradients, texture
static Buf scene(int w, int h) {
    Buf b; b.alloc(w, h);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        float u = (x + .5f) / w, v = (y + .5f) / h, *p = b.px(x, y);
        float n = rnd(x, y, 99) * 0.08f;
        if (v > 0.7f) { p[0] = 0.35f + n; p[1] = 0.55f + n; p[2] = 0.9f; }              // sky (top, y up)
        else if (v < 0.25f) { p[0] = 0.18f + n; p[1] = 0.5f + n; p[2] = 0.12f; }         // grass
        else if (u < 0.33f) { p[0] = 0.85f; p[1] = 0.6f + n; p[2] = 0.45f; }              // skin
        else if (u < 0.5f) { p[0] = p[1] = p[2] = 0.06f + n; }                          // dark clothes
        else if (u < 0.6f) { p[0] = p[1] = p[2] = 0.98f; }                               // bright highlight
        else { p[0] = u; p[1] = v; p[2] = 1 - u; }                                       // gradient
        p[3] = 0.5f + 0.5f * u;
    }
    return b;
}
static double meanDiff(const Buf& a, const Buf& b) { double s = 0; for (size_t i = 0; i < a.p.size(); i++) if (i % 4 != 3) s += std::fabs(a.p[i] - b.p[i]); return s / (a.p.size() * 0.75); }
static double maxDiff(const Buf& a, const Buf& b) { double m = 0; for (size_t i = 0; i < a.p.size(); i++) m = std::max(m, (double)std::fabs(a.p[i] - b.p[i])); return m; }
static bool valid(const Buf& o, const Buf& in) {
    for (size_t i = 0; i < o.p.size(); i++) { float v = o.p[i]; if (!(v >= 0.f && v <= 1.f)) return false; }
    for (size_t i = 3; i < o.p.size(); i += 4) if (o.p[i] != std::min(1.f, std::max(0.f, in.p[i])) && o.p[i] != in.p[i]) return false;
    return true;
}
static Buf R(const Buf& in, const Params& p, double t = 10, double fps = 24) { Buf o; render(in, o, p, t, fps); return o; }

int main() {
    const int W = 96, H = 64;
    Buf src = scene(W, H);
    // 1. every look: valid, deterministic, changes the picture, all different
    std::vector<Buf> outs;
    for (int L = 0; L < NUM_LOOKS; L++) {
        Params p; p.look = L; Buf a = R(src, p), b = R(src, p);
        EXPECT(valid(a, src), "look %d valid range / alpha", L);
        EXPECT(maxDiff(a, b) == 0, "look %d deterministic", L);
        EXPECT(meanDiff(a, src) > 0.03, "look %d changes the picture (%.4f)", L, meanDiff(a, src));
        outs.push_back(a);
    }
    for (int i = 0; i < NUM_LOOKS; i++) for (int j = i + 1; j < NUM_LOOKS; j++) EXPECT(meanDiff(outs[i], outs[j]) > 0.02, "looks %d and %d differ (%.4f)", i, j, meanDiff(outs[i], outs[j]));

    // 2. every slider changes every look (0 vs 1), and stays valid at both ends
    Buf src2 = scene(320, 200);
    const char* names[] = {"amount", "grain", "glow", "wobble", "damage", "realColor", "vignette"};
    for (int L = 0; L < NUM_LOOKS; L++) for (int s = 0; s < 7; s++) {
        Params lo, hi; lo.look = hi.look = L;
        float* fl[] = {&lo.amount, &lo.grain, &lo.glow, &lo.wobble, &lo.damage, &lo.real, &lo.vignette};
        float* fh[] = {&hi.amount, &hi.grain, &hi.glow, &hi.wobble, &hi.damage, &hi.real, &hi.vignette};
        *fl[s] = 0; *fh[s] = 1;
        Buf a = R(src2, lo), b = R(src2, hi);
        EXPECT(valid(a, src2) && valid(b, src2), "look %d %s valid at 0 and 1", L, names[s]);
        EXPECT(meanDiff(a, b) > 0.002 || (maxDiff(a, b) > 0.25 && meanDiff(a, b) > 1e-4), "look %d slider %s changes output (%.5f)", L, names[s], meanDiff(a, b));  // dust is sparse but strong
        Params mid; mid.look = L; float* fm[] = {&mid.amount, &mid.grain, &mid.glow, &mid.wobble, &mid.damage, &mid.real, &mid.vignette};
        *fm[s] = 0.25f; EXPECT(valid(R(src, mid), src), "look %d %s mid valid", L, names[s]);
    }
    // toggles
    for (int L = 0; L < NUM_LOOKS; L++) { Params a, b; a.look = b.look = L; b.lightLeaks = true; EXPECT(meanDiff(R(src, a), R(src, b)) > 0.005, "light leaks change look %d", L); }
    for (int L = 0; L < NUM_LOOKS; L++) { Params a, b; a.look = b.look = L; b.seed = 7; EXPECT(meanDiff(R(src, a), R(src, b)) > 0.0005, "seed changes look %d", L); }
    { Buf big = scene(640, 360); Params a, b; a.look = b.look = DASHCAM; b.dateStamp = false; EXPECT(meanDiff(R(big, a), R(big, b)) > 0.0005, "date stamp drawn");
      Params c = a; c.dateText = "REC 00:01"; EXPECT(meanDiff(R(big, a), R(big, c)) > 0.0002, "date text changes"); 
      Params e = a; e.dateText = std::string("\x01\xff\x7f", 3); EXPECT(valid(R(big, e), big), "weird date chars safe"); }
    { Params a, b; a.look = b.look = MELT; b.beatOn = true; b.bpm = 120; Buf x = R(src, a, 0), y = R(src, b, 0), z = R(src, b, 11);
      EXPECT(meanDiff(x, y) > 0.002, "beat hit changes Melt on the beat"); EXPECT(meanDiff(y, z) > 0.002, "Melt hit differs on/off beat");
      Params c = b; c.bpm = 90; EXPECT(meanDiff(R(src, b, 7), R(src, c, 7)) > 0.001, "BPM changes Melt"); Params d = b; d.beatOffset = 0.5f; EXPECT(meanDiff(R(src, b, 0), R(src, d, 0)) > 0.001, "beat offset changes Melt"); }
    { Params a; a.look = BOKETTO; a.beatOn = true; Params b; b.look = BOKETTO; EXPECT(maxDiff(R(src, a), R(src, b)) == 0, "beat hits only affect Melt"); }

    // 3. amount 0 = untouched; real colour 100 with effects off = untouched
    for (int L = 0; L < NUM_LOOKS; L++) {
        Params p; p.look = L; p.amount = 0; Buf o = R(src, p); EXPECT(maxDiff(o, src) < 1e-7, "look %d amount 0 is identity", L);
        Params q; q.look = L; q.real = 1; q.grain = q.glow = q.wobble = q.damage = q.vignette = 0; q.dateStamp = false;
        EXPECT(maxDiff(R(src, q), src) < 1e-5, "look %d real colour 100 + effects off is identity (%.6f)", L, maxDiff(R(src, q), src));
        Params h; h.look = L; h.amount = 0.5f; Buf full = R(src, Params{L}), half = R(src, h);
        double md = 0; for (size_t i = 0; i < src.p.size(); i++) if (i % 4 != 3) md = std::max(md, (double)std::fabs(half.p[i] - 0.5f * (src.p[i] + full.p[i]))); EXPECT(md < 1e-5, "look %d amount 50 is half mix", L);
    }
    // 4. grain is per-frame but deterministic
    for (int L = 0; L < NUM_LOOKS; L++) { Params p; p.look = L; EXPECT(meanDiff(R(src, p, 3), R(src, p, 4)) > 0.001, "look %d changes frame to frame", L); EXPECT(maxDiff(R(src, p, 4), R(src, p, 4)) == 0, "look %d same frame same pixels", L); }
    { Params p; p.look = BOKETTO; p.grain = 0; EXPECT(maxDiff(R(src, p, 3), R(src, p, 4)) == 0, "Boketto without grain is static"); }

    // 5. Super 8 frame hold
    { Params p; p.look = SUPER8; int prev = -1, changes = 0; bool ok = true;
      for (int f = 0; f < 240; f++) { double s = holdTime(p, f, 24); if (s > f || s < f - 2 || s != std::floor(s)) ok = false; if ((int)s != prev) changes++; prev = (int)s; }
      EXPECT(ok, "hold time is an earlier whole frame"); EXPECT(changes == 180, "18 fps from 24 fps: %d changes in 10 s", changes);
      int c30 = 0; prev = -1; for (int f = 0; f < 300; f++) { int s = (int)holdTime(p, f, 30); if (s != prev) c30++; prev = s; } EXPECT(c30 == 180, "18 fps from 30 fps (%d)", c30);
      p.frameHold = false; EXPECT(holdTime(p, 7, 24) == 7, "hold off"); p.frameHold = true; p.look = BOKETTO; EXPECT(holdTime(p, 7, 24) == 7, "hold only Super 8");
      p.look = SUPER8; EXPECT(holdTime(p, 7, 18) == 7 && holdTime(p, 7, 12) == 7, "no hold at 18 fps or lower"); }

    // 6. odd sizes and hostile inputs
    int sz[][2] = {{1, 1}, {2, 3}, {3, 2}, {7, 5}, {33, 17}, {17, 33}, {641, 361}};
    for (auto& s : sz) for (int L = 0; L < NUM_LOOKS; L++) {
        Buf in = scene(s[0], s[1]); Params p; p.look = L; p.lightLeaks = true; p.grain = p.glow = p.wobble = p.damage = 1; p.beatOn = true;
        Buf o = R(in, p, 5); EXPECT(o.w == s[0] && o.h == s[1] && valid(o, in), "look %d size %dx%d", L, s[0], s[1]);
    }
    { Buf in = scene(40, 30); in.p[0] = NAN; in.p[5] = INFINITY; in.p[9] = -5; in.p[14] = 7; in.p[20] = -INFINITY;
      for (int L = 0; L < NUM_LOOKS; L++) { Params p; p.look = L; Buf o = R(in, p); bool ok = true; for (float v : o.p) if (!(v >= 0 && v <= 1)) ok = false; EXPECT(ok, "look %d survives NaN/inf/out-of-range input", L); } }
    { Buf in = scene(64, 48); for (int L = 0; L < NUM_LOOKS; L++) { Params p; p.look = L; p.seed = 9999; p.bpm = 400; p.beatOffset = -16; Buf o = R(in, p, 1e6); EXPECT(valid(o, in), "look %d far frame / extreme params", L); } }
    { Buf in = scene(64, 48); Params p; p.look = 99; EXPECT(valid(R(in, p), in), "bad look index clamps"); p.look = -3; EXPECT(valid(R(in, p), in), "negative look clamps"); }
    { Buf in = scene(64, 48); Buf q8 = in; for (float& v : q8.p) v = std::round(v * 255) / 255; for (int L = 0; L < NUM_LOOKS; L++) { Params p; p.look = L; EXPECT(meanDiff(R(in, p), R(q8, p)) < 0.02, "look %d stable under 8-bit input", L); } }

    // 7. grades: range and smoothness over the whole cube
    for (int L = 0; L < NUM_LOOKS; L++) {
        bool ok = true; double maxStep = 0;
        for (int b = 0; b <= 32; b++) for (int g = 0; g <= 32; g++) for (int r = 0; r <= 32; r++) {
            float R0 = r / 32.f, G0 = g / 32.f, B0 = b / 32.f, R1 = R0, G1 = G0, B1 = B0; grade(L, R1, G1, B1);
            if (!(R1 >= 0 && R1 <= 1 && G1 >= 0 && G1 <= 1 && B1 >= 0 && B1 <= 1)) ok = false;
            if (r < 32) { float R2 = (r + 1) / 32.f, G2 = G0, B2 = B0; grade(L, R2, G2, B2); maxStep = std::max(maxStep, (double)std::fabs(R2 - R1) + std::fabs(G2 - G1) + std::fabs(B2 - B1)); }
        }
        EXPECT(ok, "grade %d in range", L);
        EXPECT(maxStep < (L == THERMAL ? 1.1 : 0.55), "grade %d has no wild jumps between LUT points (%.3f)", L, maxStep);
        float a = 0.5f, b = 0.5f, c = 0.5f; grade(L, a, b, c); EXPECT(std::fabs(a - 0.5f) + std::fabs(b - 0.5f) + std::fabs(c - 0.5f) > 0.01 || L == BOKETTO, "grade %d moves mid grey", L);
    }
    // 8. .cube text
    for (int L = 0; L < NUM_LOOKS; L++) {
        std::string t = cubeText(L); std::istringstream is(t); std::string line; int data = 0; bool size = false, ok = true, title = false; int idx = 0;
        while (std::getline(is, line)) {
            if (line.rfind("TITLE", 0) == 0) { title = true; continue; } if (line.rfind("LUT_3D_SIZE 33", 0) == 0) { size = true; continue; }
            if (line.empty() || line[0] == '#' || line.rfind("DOMAIN", 0) == 0) continue;
            float r, g, b; if (sscanf(line.c_str(), "%f %f %f", &r, &g, &b) != 3 || r < 0 || r > 1 || g < 0 || g > 1 || b < 0 || b > 1) ok = false;
            if (idx == 33 * 33 * 33 - 1 || idx == 1234) { int ri = idx % 33, gi = idx / 33 % 33, bi = idx / 1089; float R0 = ri / 32.f, G0 = gi / 32.f, B0 = bi / 32.f; grade(L, R0, G0, B0); if (std::fabs(R0 - r) > 1e-5 || std::fabs(G0 - g) > 1e-5 || std::fabs(B0 - b) > 1e-5) ok = false; }
            idx++; data++;
        }
        EXPECT(title && size && ok && data == 35937, "cube %d well-formed (%d rows)", L, data);
    }
    // 9. speed at 1080p (CPU)
    { Buf hd = scene(1920, 1080); for (int L = 0; L < NUM_LOOKS; L++) { Params p; p.look = L; auto t0 = std::chrono::steady_clock::now(); Buf o = R(hd, p); double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); printf("  1080p %-22s %6.0f ms\n", kLookNames[L], ms); EXPECT(valid(o, hd), "1080p look %d", L); } }

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
