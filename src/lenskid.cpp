// LENSKID - film, camera and glitch looks for DaVinci Resolve (OpenFX image effect, plain C API).
// Copyright (c) 2026 koobacorp. All rights reserved.
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxProperty.h"
#include "ofxParam.h"
#include "looks.h"
#include "params.h"

#if defined(_WIN32)
#define LK_EXPORT extern "C" __declspec(dllexport)
#else
#define LK_EXPORT extern "C" __attribute__((visibility("default")))
#endif

using namespace lk;

static OfxHost* gHost = nullptr;
static const OfxPropertySuiteV1* gProp = nullptr;
static const OfxParameterSuiteV1* gParam = nullptr;
static const OfxImageEffectSuiteV1* gFx = nullptr;

static int PI(const char* n) { for (int i = 0; i < kNumParams; i++) if (!strcmp(kParams[i].name, n)) return i; return -1; }

struct Inst { OfxImageClipHandle src = nullptr, dst = nullptr; OfxParamHandle h[kNumParams] = {}; };
static Inst* getInst(OfxImageEffectHandle fx) {
    OfxPropertySetHandle p; gFx->getPropertySet(fx, &p);
    void* d = nullptr; gProp->propGetPointer(p, kOfxPropInstanceData, 0, &d);
    return (Inst*)d;
}

static OfxStatus describe(OfxImageEffectHandle fx) {
    OfxPropertySetHandle p; gFx->getPropertySet(fx, &p);
    gProp->propSetString(p, kOfxPropLabel, 0, "LENSKID");
    gProp->propSetString(p, kOfxImageEffectPluginPropGrouping, 0, "Filters");
    gProp->propSetString(p, kOfxPropPluginDescription, 0, "Seven film, camera and glitch looks: Boketto, Overexposed Dream, Dashcam Junk, Super 8, Melt Signal, Infrared Ghost, Thermal Heart.");
    gProp->propSetString(p, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
    gProp->propSetString(p, kOfxImageEffectPropSupportedContexts, 1, kOfxImageEffectContextGeneral);
    gProp->propSetString(p, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
    gProp->propSetString(p, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthShort);
    gProp->propSetString(p, kOfxImageEffectPropSupportedPixelDepths, 2, kOfxBitDepthByte);
    gProp->propSetInt(p, kOfxImageEffectPropTemporalClipAccess, 0, 1);
    gProp->propSetInt(p, kOfxImageEffectPluginPropSingleInstance, 0, 0);
    gProp->propSetInt(p, kOfxImageEffectPropSupportsTiles, 0, 0);
    gProp->propSetInt(p, kOfxImageEffectPropSupportsMultiResolution, 0, 0);
    gProp->propSetInt(p, kOfxImageEffectPluginPropHostFrameThreading, 0, 0);
    gProp->propSetString(p, kOfxImageEffectPluginRenderThreadSafety, 0, kOfxImageEffectRenderFullySafe);
    return kOfxStatOK;
}

static OfxStatus describeInContext(OfxImageEffectHandle fx, OfxPropertySetHandle) {
    OfxPropertySetHandle cp;
    gFx->clipDefine(fx, kOfxImageEffectSimpleSourceClipName, &cp);
    gProp->propSetString(cp, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gProp->propSetString(cp, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentRGB);
    gProp->propSetInt(cp, kOfxImageEffectPropTemporalClipAccess, 0, 1);
    gFx->clipDefine(fx, kOfxImageEffectOutputClipName, &cp);
    gProp->propSetString(cp, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gProp->propSetString(cp, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentRGB);
    OfxParamSetHandle ps; gFx->getParamSet(fx, &ps);
    static const char* types[] = {kOfxParamTypeGroup, kOfxParamTypeDouble, kOfxParamTypeInteger, kOfxParamTypeChoice, kOfxParamTypeBoolean, kOfxParamTypeString};
    for (int i = 0; i < kNumParams; i++) {
        const PDef& d = kParams[i];
        OfxPropertySetHandle p; gParam->paramDefine(ps, types[d.kind], d.name, &p);
        gProp->propSetString(p, kOfxPropLabel, 0, d.label);
        if (d.hint && *d.hint) gProp->propSetString(p, kOfxParamPropHint, 0, d.hint);
        if (d.group) gProp->propSetString(p, kOfxParamPropParent, 0, d.group);
        if (d.kind == K_GROUP) { gProp->propSetInt(p, kOfxParamPropGroupOpen, 0, 0); continue; }  // "More options" starts closed
        if (d.kind == K_DOUBLE) {
            gProp->propSetDouble(p, kOfxParamPropDefault, 0, d.def);
            gProp->propSetDouble(p, kOfxParamPropMin, 0, d.mn); gProp->propSetDouble(p, kOfxParamPropMax, 0, d.mx);
            gProp->propSetDouble(p, kOfxParamPropDisplayMin, 0, d.mn); gProp->propSetDouble(p, kOfxParamPropDisplayMax, 0, d.mx);
        } else if (d.kind == K_INT) {
            gProp->propSetInt(p, kOfxParamPropDefault, 0, (int)d.def);
            gProp->propSetInt(p, kOfxParamPropMin, 0, (int)d.mn); gProp->propSetInt(p, kOfxParamPropMax, 0, (int)d.mx);
            gProp->propSetInt(p, kOfxParamPropDisplayMin, 0, (int)d.mn); gProp->propSetInt(p, kOfxParamPropDisplayMax, 0, (int)d.mx);
        } else if (d.kind == K_BOOL) {
            gProp->propSetInt(p, kOfxParamPropDefault, 0, (int)d.def);
        } else if (d.kind == K_CHOICE) {
            std::string o = d.options; int k = 0; size_t a = 0;
            while (true) { size_t b = o.find('|', a); gProp->propSetString(p, kOfxParamPropChoiceOption, k++, o.substr(a, b - a).c_str()); if (b == std::string::npos) break; a = b + 1; }
            gProp->propSetInt(p, kOfxParamPropDefault, 0, (int)d.def);
        } else if (d.kind == K_TEXT) {
            gProp->propSetString(p, kOfxParamPropStringMode, 0, kOfxParamStringIsSingleLine);
            gProp->propSetString(p, kOfxParamPropDefault, 0, kDateDefault);
        }
    }
    return kOfxStatOK;
}

static void updateEnabled(Inst* d, OfxTime t) {
    int look = 0; if (d->h[PI("look")]) gParam->paramGetValueAtTime(d->h[PI("look")], t, &look);
    struct E { const char* n; int look; } es[] = {{"frameHold", SUPER8}, {"dateStamp", DASHCAM}, {"dateText", DASHCAM}, {"beatOn", MELT}, {"bpm", MELT}, {"beatOffset", MELT}};
    for (const E& e : es) {
        OfxParamHandle h = d->h[PI(e.n)]; if (!h) continue;
        OfxPropertySetHandle p; if (gParam->paramGetPropertySet(h, &p) == kOfxStatOK) gProp->propSetInt(p, kOfxParamPropEnabled, 0, look == e.look ? 1 : 0);
    }
}

static OfxStatus createInstance(OfxImageEffectHandle fx) {
    Inst* d = new Inst;
    gFx->clipGetHandle(fx, kOfxImageEffectSimpleSourceClipName, &d->src, nullptr);
    gFx->clipGetHandle(fx, kOfxImageEffectOutputClipName, &d->dst, nullptr);
    OfxParamSetHandle ps; gFx->getParamSet(fx, &ps);
    for (int i = 0; i < kNumParams; i++) if (kParams[i].kind != K_GROUP) gParam->paramGetHandle(ps, kParams[i].name, &d->h[i], nullptr);
    OfxPropertySetHandle p; gFx->getPropertySet(fx, &p);
    gProp->propSetPointer(p, kOfxPropInstanceData, 0, d);
    updateEnabled(d, 0);
    return kOfxStatOK;
}
static OfxStatus destroyInstance(OfxImageEffectHandle fx) { delete getInst(fx); return kOfxStatOK; }

static double fpsOf(Inst* d) {
    OfxPropertySetHandle cp; double fps = 24;
    if (gFx->clipGetPropertySet(d->src, &cp) == kOfxStatOK) gProp->propGetDouble(cp, kOfxImageEffectPropFrameRate, 0, &fps);
    return fps > 0 ? fps : 24;
}
static Params readParams(Inst* d, OfxTime t) {
    Params p;
    auto D = [&](const char* n, double def) { double v = def; int i = PI(n); if (i >= 0 && d->h[i]) gParam->paramGetValueAtTime(d->h[i], t, &v); return v; };
    auto I = [&](const char* n, int def) { int v = def; int i = PI(n); if (i >= 0 && d->h[i]) gParam->paramGetValueAtTime(d->h[i], t, &v); return v; };
    p.look = std::max(0, std::min(NUM_LOOKS - 1, I("look", 0)));
    p.amount = (float)(D("amount", 100) / 100); p.grain = (float)(D("grain", 50) / 100); p.glow = (float)(D("glow", 50) / 100);
    p.wobble = (float)(D("wobble", 50) / 100); p.damage = (float)(D("damage", 50) / 100); p.real = (float)(D("realColor", 0) / 100);
    p.vignette = (float)(D("vignette", 50) / 100);
    p.frameHold = I("frameHold", 1) != 0; p.lightLeaks = I("lightLeaks", 0) != 0; p.dateStamp = I("dateStamp", 1) != 0; p.beatOn = I("beatOn", 0) != 0;
    p.bpm = (float)D("bpm", 120); p.beatOffset = (float)D("beatOffset", 0); p.seed = I("seed", 0);
    int ti = PI("dateText"); char* s = nullptr;
    if (ti >= 0 && d->h[ti] && gParam->paramGetValueAtTime(d->h[ti], t, &s) == kOfxStatOK && s) p.dateText = s;
    if (p.dateText.size() > 40) p.dateText.resize(40);
    return p;
}

static OfxStatus getFramesNeeded(OfxImageEffectHandle fx, OfxPropertySetHandle in, OfxPropertySetHandle out) {
    Inst* d = getInst(fx); OfxTime t = 0; gProp->propGetDouble(in, kOfxPropTime, 0, &t);
    double st = holdTime(readParams(d, t), t, fpsOf(d));
    std::string key = std::string("OfxImageClipPropFrameRange_") + kOfxImageEffectSimpleSourceClipName;
    gProp->propSetDouble(out, key.c_str(), 0, st); gProp->propSetDouble(out, key.c_str(), 1, st);
    return kOfxStatOK;
}
static OfxStatus getClipPreferences(OfxImageEffectHandle, OfxPropertySetHandle, OfxPropertySetHandle out) {
    gProp->propSetInt(out, kOfxImageEffectFrameVarying, 0, 1);  // grain, dust and flicker change every frame
    return kOfxStatOK;
}
static OfxStatus isIdentity(OfxImageEffectHandle fx, OfxPropertySetHandle in, OfxPropertySetHandle out) {
    Inst* d = getInst(fx); OfxTime t = 0; gProp->propGetDouble(in, kOfxPropTime, 0, &t);
    Params p = readParams(d, t);
    if (p.amount <= 0) {
        gProp->propSetString(out, kOfxPropName, 0, kOfxImageEffectSimpleSourceClipName);
        gProp->propSetDouble(out, kOfxPropTime, 0, t);
        return kOfxStatOK;
    }
    return kOfxStatReplyDefault;
}

struct Img { OfxPropertySetHandle h = nullptr; char* data = nullptr; OfxRectI b{}; int rb = 0, comps = 4, depth = 4; };
static bool openImg(OfxImageClipHandle c, OfxTime t, Img& im) {
    if (gFx->clipGetImage(c, t, nullptr, &im.h) != kOfxStatOK || !im.h) { im.h = nullptr; return false; }
    void* p = nullptr; gProp->propGetPointer(im.h, kOfxImagePropData, 0, &p); im.data = (char*)p;
    gProp->propGetIntN(im.h, kOfxImagePropBounds, 4, &im.b.x1);
    gProp->propGetInt(im.h, kOfxImagePropRowBytes, 0, &im.rb);
    char* s = nullptr;
    gProp->propGetString(im.h, kOfxImageEffectPropComponents, 0, &s);
    im.comps = (s && !strcmp(s, kOfxImageComponentRGB)) ? 3 : (s && !strcmp(s, kOfxImageComponentAlpha)) ? 1 : 4;
    gProp->propGetString(im.h, kOfxImageEffectPropPixelDepth, 0, &s);
    im.depth = (s && !strcmp(s, kOfxBitDepthByte)) ? 1 : (s && !strcmp(s, kOfxBitDepthShort)) ? 2 : 4;
    return im.data != nullptr;
}
static void closeImg(Img& im) { if (im.h) gFx->clipReleaseImage(im.h); im.h = nullptr; }

template <class T> static float scaleOf() { return 1.f; }
template <> float scaleOf<unsigned char>() { return 255.f; }
template <> float scaleOf<unsigned short>() { return 65535.f; }
template <class T> static void toBuf(const Img& im, Buf& b) {
    int W = im.b.x2 - im.b.x1, H = im.b.y2 - im.b.y1; b.alloc(W, H); float s = 1.f / scaleOf<T>();
    for (int y = 0; y < H; y++) { const T* r = (const T*)(im.data + (ptrdiff_t)y * im.rb);
        for (int x = 0; x < W; x++) { float* q = b.px(x, y);
            if (im.comps == 1) { q[0] = q[1] = q[2] = 0; q[3] = r[x] * s; continue; }
            for (int c = 0; c < 3; c++) q[c] = r[x * im.comps + c] * s;
            q[3] = im.comps == 4 ? r[x * 4 + 3] * s : 1.f; } }
}
template <class T> static void fromBuf(const Buf& b, int bx, int by, Img& dst, const OfxRectI& w) {
    float s = scaleOf<T>(); bool isInt = s > 1;
    for (int y = std::max(w.y1, dst.b.y1); y < std::min(w.y2, dst.b.y2); y++) {
        T* r = (T*)(dst.data + (ptrdiff_t)(y - dst.b.y1) * dst.rb);
        for (int x = std::max(w.x1, dst.b.x1); x < std::min(w.x2, dst.b.x2); x++) {
            int sx = x - bx, sy = y - by; float px[4] = {0, 0, 0, 0};
            if (sx >= 0 && sy >= 0 && sx < b.w && sy < b.h) { const float* q = b.px(sx, sy); for (int c = 0; c < 4; c++) px[c] = q[c]; }
            T* o = r + (ptrdiff_t)(x - dst.b.x1) * dst.comps;
            if (dst.comps == 1) { o[0] = (T)(isInt ? std::lround(clamp01(px[3]) * s) : px[3]); continue; }
            for (int c = 0; c < dst.comps; c++) { float v = px[c]; o[c] = isInt ? (T)std::lround(clamp01(v) * s) : (T)v; }
        }
    }
}

static OfxStatus render(OfxImageEffectHandle fx, OfxPropertySetHandle in) {
    Inst* d = getInst(fx);
    OfxTime t = 0; gProp->propGetDouble(in, kOfxPropTime, 0, &t);
    OfxRectI w; gProp->propGetIntN(in, kOfxImageEffectPropRenderWindow, 4, &w.x1);
    Params p = readParams(d, t); double fps = fpsOf(d);
    Img dst, src;
    if (!openImg(d->dst, t, dst)) return kOfxStatFailed;
    if (!openImg(d->src, holdTime(p, t, fps), src) && !openImg(d->src, t, src)) { closeImg(dst); return kOfxStatFailed; }
    Buf a, o;
    switch (src.depth) { case 1: toBuf<unsigned char>(src, a); break; case 2: toBuf<unsigned short>(src, a); break; default: toBuf<float>(src, a); }
    lk::render(a, o, p, t, fps);
    switch (dst.depth) { case 1: fromBuf<unsigned char>(o, src.b.x1, src.b.y1, dst, w); break; case 2: fromBuf<unsigned short>(o, src.b.x1, src.b.y1, dst, w); break; default: fromBuf<float>(o, src.b.x1, src.b.y1, dst, w); }
    closeImg(src); closeImg(dst);
    return kOfxStatOK;
}

static OfxStatus instanceChanged(OfxImageEffectHandle fx, OfxPropertySetHandle in) {
    char* name = nullptr; gProp->propGetString(in, kOfxPropName, 0, &name);
    if (!name || strcmp(name, "look")) return kOfxStatReplyDefault;
    OfxTime t = 0; gProp->propGetDouble(in, kOfxPropTime, 0, &t);
    updateEnabled(getInst(fx), t);
    return kOfxStatOK;
}

static OfxStatus mainEntry(const char* action, const void* handle, OfxPropertySetHandle in, OfxPropertySetHandle out) {
    OfxImageEffectHandle fx = (OfxImageEffectHandle)handle;
    try {
        if (!strcmp(action, kOfxActionLoad)) {
            if (!gHost) return kOfxStatErrMissingHostFeature;
            gProp = (const OfxPropertySuiteV1*)gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1);
            gParam = (const OfxParameterSuiteV1*)gHost->fetchSuite(gHost->host, kOfxParameterSuite, 1);
            gFx = (const OfxImageEffectSuiteV1*)gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1);
            return (gProp && gParam && gFx) ? kOfxStatOK : kOfxStatErrMissingHostFeature;
        }
        if (!strcmp(action, kOfxActionUnload)) return kOfxStatOK;
        if (!strcmp(action, kOfxActionDescribe)) return describe(fx);
        if (!strcmp(action, kOfxImageEffectActionDescribeInContext)) return describeInContext(fx, in);
        if (!strcmp(action, kOfxActionCreateInstance)) return createInstance(fx);
        if (!strcmp(action, kOfxActionDestroyInstance)) return destroyInstance(fx);
        if (!strcmp(action, kOfxImageEffectActionRender)) return render(fx, in);
        if (!strcmp(action, kOfxImageEffectActionIsIdentity)) return isIdentity(fx, in, out);
        if (!strcmp(action, kOfxImageEffectActionGetFramesNeeded)) return getFramesNeeded(fx, in, out);
        if (!strcmp(action, kOfxImageEffectActionGetClipPreferences)) return getClipPreferences(fx, in, out);
        if (!strcmp(action, kOfxActionInstanceChanged)) return instanceChanged(fx, in);
    } catch (...) { return kOfxStatFailed; }
    return kOfxStatReplyDefault;
}

static void setHost(OfxHost* h) { gHost = h; }
static OfxPlugin gPlugin = {kOfxImageEffectPluginApi, 1, "com.koobacorp.lenskid", 1, 0, setHost, mainEntry};
LK_EXPORT int OfxGetNumberOfPlugins(void) { return 1; }
LK_EXPORT OfxPlugin* OfxGetPlugin(int n) { return n == 0 ? &gPlugin : nullptr; }
LK_EXPORT OfxStatus OfxSetHost(const OfxHost* host) { gHost = (OfxHost*)host; return kOfxStatOK; }
