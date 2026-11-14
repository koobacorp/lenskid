// Writes the LENSKID .cube LUT pack (33-point, Rec.709 in/out): make_luts <folder>
#include <cstdio>
#include <string>
#include "looks.h"
int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "LENSKID";
    for (int L = 0; L < lk::NUM_LOOKS; L++) {
        std::string p = dir + "/" + lk::kLookFiles[L] + ".cube";
        FILE* f = fopen(p.c_str(), "wb"); if (!f) { fprintf(stderr, "cannot write %s\n", p.c_str()); return 1; }
        std::string t = lk::cubeText(L); fwrite(t.data(), 1, t.size(), f); fclose(f);
        printf("%s\n", p.c_str());
    }
    return 0;
}
