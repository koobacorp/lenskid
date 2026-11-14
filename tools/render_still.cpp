// Renders one LENSKID look on a PPM photo: render_still in.ppm out.ppm look [frame] [key=value ...]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "looks.h"
using namespace lk;
static bool readPPM(const char* p, Buf& b) {
    FILE* f = fopen(p, "rb"); if (!f) return false;
    int w, h, m; if (fscanf(f, "P6 %d %d %d", &w, &h, &m) != 3) { fclose(f); return false; } fgetc(f);
    std::vector<unsigned char> d((size_t)w * h * 3); size_t n = fread(d.data(), 1, d.size(), f); fclose(f); if (n != d.size()) return false;
    b.alloc(w, h);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { float* q = b.px(x, h - 1 - y); for (int c = 0; c < 3; c++) q[c] = d[((size_t)y * w + x) * 3 + c] / 255.f; q[3] = 1; }
    return true;
}
static void writePPM(const char* p, const Buf& b) {
    FILE* f = fopen(p, "wb"); fprintf(f, "P6\n%d %d\n255\n", b.w, b.h);
    for (int y = b.h - 1; y >= 0; y--) for (int x = 0; x < b.w; x++) for (int c = 0; c < 3; c++) fputc((int)std::lround(clamp01(b.px(x, y)[c]) * 255), f);
    fclose(f);
}
int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: render_still in.ppm out.ppm look [frame] [key=value...]\n"); return 2; }
    Buf in, out; if (!readPPM(argv[1], in)) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    Params p; p.look = atoi(argv[3]); double t = 0;
    for (int i = 4; i < argc; i++) {
        const char* a = argv[i]; const char* e = strchr(a, '=');
        if (!e) { t = atof(a); continue; }
        std::string k(a, e - a); float v = (float)atof(e + 1);
        if (k == "amount") p.amount = v; else if (k == "grain") p.grain = v; else if (k == "glow") p.glow = v; else if (k == "wobble") p.wobble = v;
        else if (k == "damage") p.damage = v; else if (k == "real") p.real = v; else if (k == "vignette") p.vignette = v; else if (k == "leaks") p.lightLeaks = v > 0;
        else if (k == "date") p.dateStamp = v > 0; else if (k == "beat") p.beatOn = v > 0; else if (k == "seed") p.seed = (int)v;
    }
    bool gradeOnly = false;
    for (int i = 4; i < argc; i++) if (!strcmp(argv[i], "gradeonly=1")) gradeOnly = true;
    if (gradeOnly) { out = in; for (int y = 0; y < in.h; y++) for (int x = 0; x < in.w; x++) { float* q = out.px(x, y); grade(p.look, q[0], q[1], q[2], 0.5f); } }
    else render(in, out, p, t, 24);
    writePPM(argv[2], out);
    return 0;
}
