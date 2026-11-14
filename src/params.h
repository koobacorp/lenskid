// LENSKID parameter table (shared by the plugin and the host test).
#pragma once
#include "looks.h"
namespace lk {
enum Kind { K_GROUP, K_DOUBLE, K_INT, K_CHOICE, K_BOOL, K_TEXT };
struct PDef { const char* name; Kind kind; const char* label; const char* hint; const char* group; double def, mn, mx; const char* options; };
#define LOOKS "Boketto Moody Vintage|Overexposed Dream|Dashcam Junk|Super 8|Melt Signal|Infrared Ghost|Thermal Heart"
static const PDef kParams[] = {
    {"look", K_CHOICE, "Look", "Pick a look. Every slider's 50 is that look's normal amount", 0, 0, 0, 0, LOOKS},
    {"amount", K_DOUBLE, "Amount %", "How much of the whole look. 0 = the clip untouched", 0, 100, 0, 100, 0},
    {"grain", K_DOUBLE, "Grain", "Film grain / sensor noise. 50 = normal, 0 = none, 100 = double", 0, 50, 0, 100, 0},
    {"glow", K_DOUBLE, "Glow / Halation", "Glow and coloured halo around bright parts. 50 = normal", 0, 50, 0, 100, 0},
    {"wobble", K_DOUBLE, "Wobble / Lens", "Boketto: soft vintage-lens edges. Dream: mist. Dashcam: fisheye bulge. Super 8: gate weave and flicker. Melt: row slides. Infrared: ghost distance. Thermal: heat shimmer", 0, 50, 0, 100, 0},
    {"damage", K_DOUBLE, "Distortion / Damage", "Boketto: faded blacks. Dream: overexposure. Dashcam: low-res blocks and over-sharpening. Super 8: dust and scratches. Melt: RGB tearing and colour bleed. Infrared: double exposure. Thermal: low-res sensor and heat bands", 0, 50, 0, 100, 0},
    {"realColor", K_DOUBLE, "Real colour blend %", "Blends the look's colours back toward the real colours (keeps the grain, glow and damage)", 0, 0, 0, 100, 0},
    {"grpMore", K_GROUP, "More options", 0, 0, 0, 0, 0, 0},
    {"vignette", K_DOUBLE, "Vignette", "Dark corners. 50 = the look's normal amount", "grpMore", 50, 0, 100, 0},
    {"frameHold", K_BOOL, "18 fps feel (Super 8)", "Super 8 only: holds frames so motion steps like 18 fps film", "grpMore", 1, 0, 1, 0},
    {"lightLeaks", K_BOOL, "Light leaks", "Warm orange light leaks drifting across the frame (any look)", "grpMore", 0, 0, 1, 0},
    {"dateStamp", K_BOOL, "Date stamp (Dashcam)", "Dashcam only: orange date stamp in the corner", "grpMore", 1, 0, 1, 0},
    {"dateText", K_TEXT, "Date stamp text", "What the stamp says", "grpMore", 0, 0, 0, 0},
    {"beatOn", K_BOOL, "Beat hits (Melt)", "Melt only: the tearing hits hard on every beat of your BPM", "grpMore", 0, 0, 1, 0},
    {"bpm", K_DOUBLE, "BPM", "Your song's tempo, for Beat hits", "grpMore", 120, 20, 400, 0},
    {"beatOffset", K_DOUBLE, "Beat offset (beats)", "Shifts where the hits land. 0.5 = half a beat later", "grpMore", 0, -16, 16, 0},
    {"seed", K_INT, "Random seed", "Changes the grain, dust and glitch pattern", "grpMore", 0, 0, 9999, 0},
};
static const int kNumParams = sizeof(kParams) / sizeof(kParams[0]);
static const char* const kDateDefault = "2026.11.14  21:07";
}  // namespace lk
