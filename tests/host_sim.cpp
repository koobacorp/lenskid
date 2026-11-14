// Minimal OpenFX host simulator: loads the real LENSKID plugin binary and drives it like a host
// (load, describe, create instance, frames-needed, is-identity, instance-changed, render) at 8/16/32-bit.
#include <dlfcn.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxProperty.h"
#include "ofxParam.h"
#include "looks.h"
#include "params.h"
#include <chrono>
#include <thread>
#include <algorithm>

struct Val { std::vector<std::string> s; std::vector<double> d; std::vector<int> i; std::vector<void*> p; };
struct PS { std::map<std::string, Val> m; };
#define H(x) ((PS*)(x))
template <class V, class T> static void put(V& v, int i, T x) { if ((int)v.size() <= i) v.resize(i + 1); v[i] = x; }
static OfxStatus sP(OfxPropertySetHandle h, const char* n, int i, void* v) { put(H(h)->m[n].p, i, v); return kOfxStatOK; }
static OfxStatus sS(OfxPropertySetHandle h, const char* n, int i, const char* v) { put(H(h)->m[n].s, i, std::string(v)); return kOfxStatOK; }
static OfxStatus sD(OfxPropertySetHandle h, const char* n, int i, double v) { put(H(h)->m[n].d, i, v); return kOfxStatOK; }
static OfxStatus sI(OfxPropertySetHandle h, const char* n, int i, int v) { put(H(h)->m[n].i, i, v); return kOfxStatOK; }
static OfxStatus sPN(OfxPropertySetHandle h, const char* n, int c, void* const* v) { for (int i = 0; i < c; i++) sP(h, n, i, v[i]); return kOfxStatOK; }
static OfxStatus sSN(OfxPropertySetHandle h, const char* n, int c, const char* const* v) { for (int i = 0; i < c; i++) sS(h, n, i, v[i]); return kOfxStatOK; }
static OfxStatus sDN(OfxPropertySetHandle h, const char* n, int c, const double* v) { for (int i = 0; i < c; i++) sD(h, n, i, v[i]); return kOfxStatOK; }
static OfxStatus sIN(OfxPropertySetHandle h, const char* n, int c, const int* v) { for (int i = 0; i < c; i++) sI(h, n, i, v[i]); return kOfxStatOK; }
static Val* fv(OfxPropertySetHandle h, const char* n) { auto it = H(h)->m.find(n); return it == H(h)->m.end() ? nullptr : &it->second; }
static OfxStatus gP(OfxPropertySetHandle h, const char* n, int i, void** v) { Val* x = fv(h, n); if (!x || (int)x->p.size() <= i) return kOfxStatErrUnknown; *v = x->p[i]; return kOfxStatOK; }
static OfxStatus gS(OfxPropertySetHandle h, const char* n, int i, char** v) { Val* x = fv(h, n); if (!x || (int)x->s.size() <= i) return kOfxStatErrUnknown; *v = (char*)x->s[i].c_str(); return kOfxStatOK; }
static OfxStatus gD(OfxPropertySetHandle h, const char* n, int i, double* v) { Val* x = fv(h, n); if (!x || (int)x->d.size() <= i) return kOfxStatErrUnknown; *v = x->d[i]; return kOfxStatOK; }
static OfxStatus gI(OfxPropertySetHandle h, const char* n, int i, int* v) { Val* x = fv(h, n); if (!x || (int)x->i.size() <= i) return kOfxStatErrUnknown; *v = x->i[i]; return kOfxStatOK; }
static OfxStatus gPN(OfxPropertySetHandle h, const char* n, int c, void** v) { for (int i = 0; i < c; i++) if (gP(h, n, i, v + i)) return kOfxStatErrUnknown; return kOfxStatOK; }
static OfxStatus gSN(OfxPropertySetHandle h, const char* n, int c, char** v) { for (int i = 0; i < c; i++) if (gS(h, n, i, v + i)) return kOfxStatErrUnknown; return kOfxStatOK; }
static OfxStatus gDN(OfxPropertySetHandle h, const char* n, int c, double* v) { for (int i = 0; i < c; i++) if (gD(h, n, i, v + i)) return kOfxStatErrUnknown; return kOfxStatOK; }
static OfxStatus gIN(OfxPropertySetHandle h, const char* n, int c, int* v) { for (int i = 0; i < c; i++) if (gI(h, n, i, v + i)) return kOfxStatErrUnknown; return kOfxStatOK; }
static OfxStatus pReset(OfxPropertySetHandle h, const char* n) { H(h)->m.erase(n); return kOfxStatOK; }
static OfxStatus pDim(OfxPropertySetHandle h, const char* n, int* c) { Val* x = fv(h, n); *c = x ? (int)std::max({x->s.size(), x->d.size(), x->i.size(), x->p.size()}) : 0; return kOfxStatOK; }
static OfxPropertySuiteV1 propSuite = {sP, sS, sD, sI, sPN, sSN, sDN, sIN, gP, gS, gD, gI, gPN, gSN, gDN, gIN, pReset, pDim};

// ---- params ----
struct Param { std::string type, name; PS props; double d = 0; int i = 0; std::string str; };
struct ParamSet { PS props; std::map<std::string, std::unique_ptr<Param>> ps; std::vector<std::string> order; };
static OfxStatus paramDefine(OfxParamSetHandle s, const char* type, const char* name, OfxPropertySetHandle* p) {
    auto* S = (ParamSet*)s; auto pr = std::make_unique<Param>(); pr->type = type; pr->name = name;
    if (p) *p = (OfxPropertySetHandle)&pr->props; S->order.push_back(name); S->ps[name] = std::move(pr); return kOfxStatOK;
}
static OfxStatus paramGetHandle(OfxParamSetHandle s, const char* name, OfxParamHandle* h, OfxPropertySetHandle* p) {
    auto* S = (ParamSet*)s; auto it = S->ps.find(name); if (it == S->ps.end()) return kOfxStatErrUnknown;
    *h = (OfxParamHandle)it->second.get(); if (p) *p = (OfxPropertySetHandle)&it->second->props; return kOfxStatOK;
}
static bool isInt(const Param* p) { return p->type == kOfxParamTypeInteger || p->type == kOfxParamTypeChoice || p->type == kOfxParamTypeBoolean; }
static OfxStatus getV(Param* p, va_list a) {
    if (p->type == kOfxParamTypeString) { *va_arg(a, char**) = (char*)p->str.c_str(); return kOfxStatOK; }
    if (isInt(p)) *va_arg(a, int*) = p->i; else if (p->type == kOfxParamTypeDouble) *va_arg(a, double*) = p->d; else return kOfxStatErrBadHandle;
    return kOfxStatOK;
}
static OfxStatus paramGetValue(OfxParamHandle h, ...) { va_list a; va_start(a, h); OfxStatus r = getV((Param*)h, a); va_end(a); return r; }
static OfxStatus paramGetValueAtTime(OfxParamHandle h, OfxTime t, ...) { va_list a; va_start(a, t); OfxStatus r = getV((Param*)h, a); va_end(a); return r; }
static OfxStatus paramSetValue(OfxParamHandle h, ...) { auto* p = (Param*)h; va_list a; va_start(a, h); if (p->type == kOfxParamTypeString) p->str = va_arg(a, const char*); else if (isInt(p)) p->i = va_arg(a, int); else p->d = va_arg(a, double); va_end(a); return kOfxStatOK; }
static OfxStatus paramGetPS(OfxParamHandle h, OfxPropertySetHandle* p) { *p = (OfxPropertySetHandle)&((Param*)h)->props; return kOfxStatOK; }
static OfxParameterSuiteV1 paramSuite;

// ---- clips / images ----
static int W = 64, Ht = 36, X0 = 0, Y0 = 0, gDepth = 4, gComps = 4;  // image size, bounds origin, bytes per channel, channels
struct Clip { PS props; };
struct Effect { PS props; ParamSet params; std::map<std::string, std::unique_ptr<Clip>> clips; };
struct ImgBuf { PS props; std::vector<unsigned char> px; double t; };
static std::vector<ImgBuf*> gLive;
static std::vector<double> gFetched;
static Clip* gOutClip = nullptr;
static ImgBuf* gLastOut = nullptr;
static float srcPixel(int x, int y, int c) {  // sky / foliage / skin / dark / highlight / gradient (y up)
    float u = (x + .5f) / W, v = (y + .5f) / Ht, n = lk::rnd(x, y, 5) * 0.06f;
    if (c == 3) return 0.5f + 0.5f * u;
    static const float sky[3] = {.35f, .55f, .9f}, grass[3] = {.18f, .5f, .12f}, skin[3] = {.85f, .6f, .45f};
    if (v > 0.7f) return sky[c] + n; if (v < 0.25f) return grass[c] + n; if (u < 0.33f) return skin[c]; if (u < 0.5f) return 0.06f + n; if (u < 0.6f) return 0.98f;
    return c == 0 ? u : c == 1 ? v : 1 - u;
}
static double gMaxVal() { return gDepth == 1 ? 255.0 : gDepth == 2 ? 65535.0 : 1.0; }
static void putPx(ImgBuf* im, int x, int y, int c, double v) {
    size_t i = ((size_t)y * W + x) * gComps + c;
    if (gDepth == 1) im->px[i] = (unsigned char)std::lround(std::min(1.0, std::max(0.0, v)) * 255);
    else if (gDepth == 2) ((unsigned short*)im->px.data())[i] = (unsigned short)std::lround(std::min(1.0, std::max(0.0, v)) * 65535);
    else ((float*)im->px.data())[i] = (float)v;
}
static double getPx(const ImgBuf* im, int x, int y, int c) {
    size_t i = ((size_t)y * W + x) * gComps + c;
    if (gDepth == 1) return im->px[i] / 255.0; if (gDepth == 2) return ((const unsigned short*)im->px.data())[i] / 65535.0; return ((const float*)im->px.data())[i];
}
static OfxStatus getPropertySet(OfxImageEffectHandle e, OfxPropertySetHandle* p) { *p = (OfxPropertySetHandle)&((Effect*)e)->props; return kOfxStatOK; }
static OfxStatus getParamSet(OfxImageEffectHandle e, OfxParamSetHandle* p) { *p = (OfxParamSetHandle)&((Effect*)e)->params; return kOfxStatOK; }
static OfxStatus clipDefine(OfxImageEffectHandle e, const char* n, OfxPropertySetHandle* p) { auto c = std::make_unique<Clip>(); *p = (OfxPropertySetHandle)&c->props; ((Effect*)e)->clips[n] = std::move(c); return kOfxStatOK; }
static OfxStatus clipGetHandle(OfxImageEffectHandle e, const char* n, OfxImageClipHandle* c, OfxPropertySetHandle* p) {
    auto& m = ((Effect*)e)->clips; auto it = m.find(n); if (it == m.end()) return kOfxStatErrUnknown;
    *c = (OfxImageClipHandle)it->second.get(); if (p) *p = (OfxPropertySetHandle)&it->second->props; return kOfxStatOK;
}
static OfxStatus clipGetPropertySet(OfxImageClipHandle c, OfxPropertySetHandle* p) { *p = (OfxPropertySetHandle)&((Clip*)c)->props; return kOfxStatOK; }
static OfxStatus clipGetImage(OfxImageClipHandle c, OfxTime t, const OfxRectD*, OfxPropertySetHandle* out) {
    auto* im = new ImgBuf; im->t = t; im->px.assign((size_t)W * Ht * gComps * gDepth, 0);
    bool isOut = (Clip*)c == gOutClip;
    if (!isOut) { gFetched.push_back(t); for (int y = 0; y < Ht; y++) for (int x = 0; x < W; x++) for (int k = 0; k < gComps; k++) putPx(im, x, y, k, srcPixel(x, y, k)); }
    else { for (int y = 0; y < Ht; y++) for (int x = 0; x < W; x++) for (int k = 0; k < gComps; k++) putPx(im, x, y, k, gDepth == 4 ? -7.0 : 0.0); gLastOut = im; }  // sentinel
    gLive.push_back(im);
    OfxPropertySetHandle h = (OfxPropertySetHandle)&im->props;
    sP(h, kOfxImagePropData, 0, im->px.data()); int b[4] = {X0, Y0, X0 + W, Y0 + Ht}; sIN(h, kOfxImagePropBounds, 4, b);
    sI(h, kOfxImagePropRowBytes, 0, W * gComps * gDepth); sS(h, kOfxImageEffectPropComponents, 0, gComps == 4 ? kOfxImageComponentRGBA : kOfxImageComponentRGB);
    sS(h, kOfxImageEffectPropPixelDepth, 0, gDepth == 1 ? kOfxBitDepthByte : gDepth == 2 ? kOfxBitDepthShort : kOfxBitDepthFloat);
    *out = h; return kOfxStatOK;
}
static OfxStatus clipReleaseImage(OfxPropertySetHandle) { return kOfxStatOK; }
static OfxImageEffectSuiteV1 fxSuite;
static const void* fetchSuite(OfxPropertySetHandle, const char* n, int v) {
    if (!strcmp(n, kOfxPropertySuite) && v == 1) return &propSuite;
    if (!strcmp(n, kOfxParameterSuite) && v == 1) return &paramSuite;
    if (!strcmp(n, kOfxImageEffectSuite) && v == 1) return &fxSuite;
    return nullptr;
}
static PS hostProps;
static OfxHost host = {(OfxPropertySetHandle)&hostProps, fetchSuite};
static int fails = 0, checks = 0;
#define EXPECT(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(int argc, char** argv) {
    paramSuite = {}; paramSuite.paramDefine = paramDefine; paramSuite.paramGetHandle = paramGetHandle; paramSuite.paramGetValue = paramGetValue;
    paramSuite.paramGetValueAtTime = paramGetValueAtTime; paramSuite.paramSetValue = paramSetValue; paramSuite.paramGetPropertySet = paramGetPS;
    fxSuite = {}; fxSuite.getPropertySet = getPropertySet; fxSuite.getParamSet = getParamSet; fxSuite.clipDefine = clipDefine;
    fxSuite.clipGetHandle = clipGetHandle; fxSuite.clipGetPropertySet = clipGetPropertySet; fxSuite.clipGetImage = clipGetImage; fxSuite.clipReleaseImage = clipReleaseImage;
    const char* lib = argc > 1 ? argv[1] : "./LENSKID.ofx";
    void* so = dlopen(lib, RTLD_NOW); if (!so) { printf("dlopen failed: %s\n", dlerror()); return 2; }
    auto num = (int (*)())dlsym(so, "OfxGetNumberOfPlugins"); auto get = (OfxPlugin * (*)(int)) dlsym(so, "OfxGetPlugin");
    EXPECT(num && get && dlsym(so, "OfxSetHost"), "entry points exported");
    if (!num || !get) return 1;
    EXPECT(num() == 1, "one plugin"); EXPECT(get(1) == nullptr, "only one plugin");
    OfxPlugin* pl = get(0);
    EXPECT(!strcmp(pl->pluginApi, kOfxImageEffectPluginApi) && pl->apiVersion == 1 && !strcmp(pl->pluginIdentifier, "com.koobacorp.lenskid") && pl->pluginVersionMajor == 1, "identity");
    pl->setHost(&host);
    EXPECT(pl->mainEntry(kOfxActionLoad, nullptr, nullptr, nullptr) == kOfxStatOK, "load");
    Effect desc; EXPECT(pl->mainEntry(kOfxActionDescribe, &desc, nullptr, nullptr) == kOfxStatOK, "describe");
    char* s = nullptr; gS((OfxPropertySetHandle)&desc.props, kOfxImageEffectPluginPropGrouping, 0, &s); EXPECT(s && !strcmp(s, "Filters"), "grouping");
    gS((OfxPropertySetHandle)&desc.props, kOfxPropLabel, 0, &s); EXPECT(s && !strcmp(s, "LENSKID"), "label");
    { Val* v = fv((OfxPropertySetHandle)&desc.props, kOfxImageEffectPropSupportedPixelDepths); EXPECT(v && v->s.size() == 3, "8/16/32-bit supported");
      Val* c = fv((OfxPropertySetHandle)&desc.props, kOfxImageEffectPropSupportedContexts); EXPECT(c && c->s.size() == 2, "filter + general contexts"); }
    Effect fx; PS ctx; sS((OfxPropertySetHandle)&ctx, kOfxImageEffectPropContext, 0, kOfxImageEffectContextFilter);
    EXPECT(pl->mainEntry(kOfxImageEffectActionDescribeInContext, &fx, (OfxPropertySetHandle)&ctx, nullptr) == kOfxStatOK, "describeInContext");
    EXPECT(fx.clips.count("Source") && fx.clips.count("Output"), "clips");
    printf("parameters:\n");
    for (auto& n : fx.params.order) { Param* p = fx.params.ps[n].get(); char* l = nullptr; gS((OfxPropertySetHandle)&p->props, kOfxPropLabel, 0, &l);
        double dd; int di; char* ds; if (!gD((OfxPropertySetHandle)&p->props, kOfxParamPropDefault, 0, &dd)) p->d = dd; if (!gI((OfxPropertySetHandle)&p->props, kOfxParamPropDefault, 0, &di)) p->i = di;
        if (!gS((OfxPropertySetHandle)&p->props, kOfxParamPropDefault, 0, &ds)) p->str = ds;
        char* parent = nullptr; gS((OfxPropertySetHandle)&p->props, kOfxParamPropParent, 0, &parent);
        printf("  %-12s %-10s %-26s %s\n", n.c_str(), p->type.c_str() + 12, l ? l : "", parent ? "(More options)" : "");
        EXPECT(l && *l, "label for %s", n.c_str()); }
    EXPECT((int)fx.params.order.size() == lk::kNumParams, "all params defined");
    { Param* g = fx.params.ps["grpMore"].get(); int open = 1; gI((OfxPropertySetHandle)&g->props, kOfxParamPropGroupOpen, 0, &open); EXPECT(open == 0, "More options starts closed"); }
    { Val* v = fv((OfxPropertySetHandle)&fx.params.ps["look"]->props, kOfxParamPropChoiceOption); EXPECT(v && v->s.size() == 7 && v->s[6] == "Thermal Heart", "7 looks in the menu"); }
    auto P = [&](const char* n) { return fx.params.ps[n].get(); };
    EXPECT(P("amount")->d == 100 && P("grain")->d == 50 && P("realColor")->d == 0 && P("look")->i == 0 && P("dateText")->str.size() > 5, "defaults");
    Clip* srcClip = fx.clips["Source"].get();
    sD((OfxPropertySetHandle)&srcClip->props, kOfxImageEffectPropFrameRate, 0, 24.0);
    gOutClip = fx.clips["Output"].get();
    EXPECT(pl->mainEntry(kOfxActionCreateInstance, &fx, nullptr, nullptr) == kOfxStatOK, "create");
    auto enabled = [&](const char* n) { int e = 1; gI((OfxPropertySetHandle)&P(n)->props, kOfxParamPropEnabled, 0, &e); return e; };
    EXPECT(!enabled("frameHold") && !enabled("dateStamp") && !enabled("beatOn"), "look-only options greyed out for Boketto");
    auto changed = [&](const char* n) { PS in; sS((OfxPropertySetHandle)&in, kOfxPropName, 0, n); sS((OfxPropertySetHandle)&in, kOfxPropType, 0, kOfxTypeParameter); sD((OfxPropertySetHandle)&in, kOfxPropTime, 0, 0); return pl->mainEntry(kOfxActionInstanceChanged, &fx, (OfxPropertySetHandle)&in, nullptr); };
    P("look")->i = lk::DASHCAM; changed("look"); EXPECT(enabled("dateStamp") && enabled("dateText") && !enabled("frameHold"), "Dashcam enables date stamp");
    P("look")->i = lk::SUPER8; changed("look"); EXPECT(enabled("frameHold") && !enabled("dateStamp"), "Super 8 enables frame hold");
    P("look")->i = lk::MELT; changed("look"); EXPECT(enabled("beatOn") && enabled("bpm"), "Melt enables beat hits");
    EXPECT(changed("grain") == kOfxStatReplyDefault, "other params: default reply");

    auto renderAt = [&](double t, int win[4] = nullptr) -> ImgBuf* {
        PS in; sD((OfxPropertySetHandle)&in, kOfxPropTime, 0, t);
        int full[4] = {X0, Y0, X0 + W, Y0 + Ht}; sIN((OfxPropertySetHandle)&in, kOfxImageEffectPropRenderWindow, 4, win ? win : full);
        double sc[2] = {1, 1}; sDN((OfxPropertySetHandle)&in, kOfxImageEffectPropRenderScale, 2, sc);
        for (auto* b : gLive) delete b; gLive.clear(); gFetched.clear(); gLastOut = nullptr;
        OfxStatus r = pl->mainEntry(kOfxImageEffectActionRender, &fx, (OfxPropertySetHandle)&in, nullptr);
        EXPECT(r == kOfxStatOK && gLastOut, "render t=%g", t); return gLastOut;
    };
    auto snapshot = [&](ImgBuf* o) { std::vector<double> v; for (int y = 0; y < Ht; y++) for (int x = 0; x < W; x++) for (int c = 0; c < gComps; c++) v.push_back(getPx(o, x, y, c)); return v; };
    auto mdiff = [](const std::vector<double>& a, const std::vector<double>& b) { double s = 0; for (size_t i = 0; i < a.size(); i++) s += std::fabs(a[i] - b[i]); return s / a.size(); };

    // every look at every bit depth and channel layout, including odd sizes with a shifted origin
    struct Cfg { int w, h, x0, y0, depth, comps; } cfgs[] = {{64, 36, 0, 0, 4, 4}, {64, 36, 0, 0, 1, 4}, {64, 36, 0, 0, 2, 4}, {64, 36, 0, 0, 4, 3}, {33, 17, 10, -5, 1, 3}, {1, 1, 0, 0, 4, 4}, {7, 3, 3, 3, 2, 4}, {641, 361, 0, 0, 4, 4}};
    for (auto& c : cfgs) {
        W = c.w; Ht = c.h; X0 = c.x0; Y0 = c.y0; gDepth = c.depth; gComps = c.comps;
        std::vector<std::vector<double>> outs;
        for (int L = 0; L < lk::NUM_LOOKS; L++) {
            P("look")->i = L; ImgBuf* o = renderAt(12); if (!o) continue;
            std::vector<double> v = snapshot(o); bool ok = true; for (double x : v) if (!(x >= 0 && x <= 1)) ok = false;
            EXPECT(ok, "look %d %dx%d depth %d comps %d: values 0..1, no NaN", L, c.w, c.h, c.depth, c.comps);
            if (gComps == 4) { bool a = true; for (int y = 0; y < Ht; y++) for (int x = 0; x < W; x++) if (std::fabs(getPx(o, x, y, 3) - (gDepth == 4 ? srcPixel(x, y, 3) : std::lround(srcPixel(x, y, 3) * gMaxVal()) / gMaxVal())) > 1e-6) a = false; EXPECT(a, "alpha kept, look %d", L); }
            ImgBuf* o2 = renderAt(12); EXPECT(mdiff(v, snapshot(o2)) == 0, "deterministic look %d", L);
            outs.push_back(v);
        }
        if (c.w >= 33) for (int i = 0; i < (int)outs.size(); i++) for (int j = i + 1; j < (int)outs.size(); j++) EXPECT(mdiff(outs[i], outs[j]) > 0.01, "looks %d/%d differ at %dx%d d%d", i, j, c.w, c.h, c.depth);
    }
    // the plugin gives exactly the engine's answer (float RGBA)
    W = 96; Ht = 54; X0 = Y0 = 0; gDepth = 4; gComps = 4;
    for (int L = 0; L < lk::NUM_LOOKS; L++) {
        P("look")->i = L; ImgBuf* o = renderAt(30);
        lk::Buf in, ref; in.alloc(W, Ht); for (int y = 0; y < Ht; y++) for (int x = 0; x < W; x++) for (int k = 0; k < 4; k++) in.px(x, y)[k] = srcPixel(x, y, k);
        lk::Params p; p.look = L; p.dateText = P("dateText")->str;
        double st = lk::holdTime(p, 30, 24); lk::render(in, ref, p, 30, 24); (void)st;
        double m = 0; for (int y = 0; y < Ht; y++) for (int x = 0; x < W; x++) for (int k = 0; k < 4; k++) m = std::max(m, std::fabs(getPx(o, x, y, k) - ref.px(x, y)[k]));
        EXPECT(m < 1e-6, "plugin output equals engine output, look %d (%.2g)", L, m);
        if (m >= 1e-6) { int cnt = 0; for (int y = 0; y < Ht; y++) for (int x = 0; x < W; x++) for (int k = 0; k < 4; k++) if (std::fabs(getPx(o, x, y, k) - ref.px(x, y)[k]) > 1e-6 && cnt++ < 3) printf("   diff at %d,%d,%d: %f vs %f\n", x, y, k, getPx(o, x, y, k), ref.px(x, y)[k]); printf("   %d diffs\n", cnt); }
    }
    // every slider and toggle reaches the engine
    struct SP { const char* n; double a, b; int look; } sps[] = {{"amount", 0, 100, 0}, {"grain", 0, 100, 3}, {"glow", 0, 100, 1}, {"wobble", 0, 100, 2}, {"damage", 0, 100, 4},
        {"realColor", 0, 100, 6}, {"vignette", 0, 100, 0}, {"lightLeaks", 0, 1, 5}, {"dateStamp", 0, 1, 2}, {"beatOn", 0, 1, 4}, {"seed", 0, 42, 3}, {"bpm", 60, 130, 4}, {"beatOffset", 0, 0.5, 4}};
    W = 320; Ht = 180;
    for (auto& sp : sps) {
        P("look")->i = sp.look; Param* q = P(sp.n); double keep = q->d; int keepi = q->i; bool beat = !strcmp(sp.n, "bpm") || !strcmp(sp.n, "beatOffset"); if (beat) P("beatOn")->i = 1;
        auto set = [&](double v) { if (isInt(q)) q->i = (int)v; else q->d = v; };
        set(sp.a); auto A = snapshot(renderAt(7)); set(sp.b); auto B = snapshot(renderAt(7));
        EXPECT(mdiff(A, B) > 1e-4, "param %s changes output (%.5f)", sp.n, mdiff(A, B));
        q->d = keep; q->i = keepi; if (beat) P("beatOn")->i = 0;
    }
    { P("look")->i = lk::DASHCAM; std::string keep = P("dateText")->str; auto A = snapshot(renderAt(3)); P("dateText")->str = "REC"; auto B = snapshot(renderAt(3)); EXPECT(mdiff(A, B) > 1e-5, "date text reaches the engine"); P("dateText")->str = keep; }
    // render window: only the window is written
    { W = 64; Ht = 36; P("look")->i = lk::SUPER8; int win[4] = {10, 5, 30, 20}; ImgBuf* o = renderAt(4, win); bool inside = true, outside = true;
      for (int y = 0; y < Ht; y++) for (int x = 0; x < W; x++) { double v = getPx(o, x, y, 0); bool in = x >= 10 && x < 30 && y >= 5 && y < 20; if (in && !(v >= 0 && v <= 1)) inside = false; if (!in && v != -7.0) outside = false; }
      EXPECT(inside && outside, "render window respected"); }
    // grain changes per frame; frames needed / Super 8 hold; identity
    { P("look")->i = lk::BOKETTO; auto A = snapshot(renderAt(10)); auto B = snapshot(renderAt(11)); EXPECT(mdiff(A, B) > 1e-4, "per-frame grain"); }
    { int fv0 = 0; PS in, out; P("look")->i = lk::SUPER8; P("frameHold")->i = 1;
      for (int f = 0; f < 48; f++) { PS i2, o2; sD((OfxPropertySetHandle)&i2, kOfxPropTime, 0, f); pl->mainEntry(kOfxImageEffectActionGetFramesNeeded, &fx, (OfxPropertySetHandle)&i2, (OfxPropertySetHandle)&o2);
        Val* v = fv((OfxPropertySetHandle)&o2, "OfxImageClipPropFrameRange_Source"); lk::Params p; p.look = lk::SUPER8;
        if (v && v->d.size() == 2 && v->d[0] == lk::holdTime(p, f, 24) && v->d[1] == v->d[0]) fv0++;
        renderAt(f); EXPECT(!gFetched.empty() && gFetched[0] == lk::holdTime(p, f, 24), "Super 8 fetches the held frame at %d", f); }
      EXPECT(fv0 == 48, "frames needed = held frame (%d/48)", fv0);
      std::vector<double> held; for (int f = 0; f < 24; f++) { PS i2, o2; sD((OfxPropertySetHandle)&i2, kOfxPropTime, 0, f); pl->mainEntry(kOfxImageEffectActionGetFramesNeeded, &fx, (OfxPropertySetHandle)&i2, (OfxPropertySetHandle)&o2); held.push_back(fv((OfxPropertySetHandle)&o2, "OfxImageClipPropFrameRange_Source")->d[0]); }
      std::sort(held.begin(), held.end()); held.erase(std::unique(held.begin(), held.end()), held.end()); EXPECT(held.size() == 18, "18 distinct frames per second at 24 fps (%zu)", held.size());
      P("frameHold")->i = 0; renderAt(5); EXPECT(gFetched[0] == 5, "hold off fetches current frame"); P("frameHold")->i = 1; }
    { PS in, out; sD((OfxPropertySetHandle)&in, kOfxPropTime, 0, 3); P("amount")->d = 0;
      EXPECT(pl->mainEntry(kOfxImageEffectActionIsIdentity, &fx, (OfxPropertySetHandle)&in, (OfxPropertySetHandle)&out) == kOfxStatOK, "amount 0 = identity");
      char* nm = nullptr; gS((OfxPropertySetHandle)&out, kOfxPropName, 0, &nm); EXPECT(nm && !strcmp(nm, "Source"), "identity clip");
      P("amount")->d = 100; PS o2; EXPECT(pl->mainEntry(kOfxImageEffectActionIsIdentity, &fx, (OfxPropertySetHandle)&in, (OfxPropertySetHandle)&o2) == kOfxStatReplyDefault, "amount 100 renders"); }
    { PS in, out; pl->mainEntry(kOfxImageEffectActionGetClipPreferences, &fx, (OfxPropertySetHandle)&in, (OfxPropertySetHandle)&out); int fvy = 0; gI((OfxPropertySetHandle)&out, kOfxImageEffectFrameVarying, 0, &fvy); EXPECT(fvy == 1, "frame varying"); }
    // 4K float timing
    { W = 3840; Ht = 2160; gDepth = 4; gComps = 4; for (int L = 0; L < lk::NUM_LOOKS; L++) { P("look")->i = L; auto t0 = std::chrono::steady_clock::now(); renderAt(1); printf("  UHD float %-22s %6.0f ms\n", lk::kLookNames[L], std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); } }
    EXPECT(pl->mainEntry(kOfxActionDestroyInstance, &fx, nullptr, nullptr) == kOfxStatOK, "destroy");
    EXPECT(pl->mainEntry(kOfxActionUnload, nullptr, nullptr, nullptr) == kOfxStatOK, "unload");
    EXPECT(pl->mainEntry("OfxActionMadeUp", &fx, nullptr, nullptr) == kOfxStatReplyDefault, "unknown action");
    for (auto* b : gLive) delete b;
    printf("host simulation: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
