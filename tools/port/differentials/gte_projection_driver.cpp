// Native side of tools\port\differentials\gte_projection.py (SDK RotTransPers,
// #371): one vector per stdin line, one line per vector with the returned
// depth, SXY2, IR0, FLAG, MAC0 and the input vector's pad.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "Core/Gte.h"

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        MATRIX matrix { };
        for (auto& row : matrix.m) {
            for (auto& element : row) {
                int value = 0;
                in >> value;
                element = static_cast<short>(value);
            }
        }
        for (auto& translation : matrix.t) {
            in >> translation;
        }
        long ofx = 0, ofy = 0, h = 0, dqa = 0, dqb = 0;
        int x = 0, y = 0, z = 0, pad = 0;
        in >> ofx >> ofy >> h >> dqa >> dqb >> x >> y >> z >> pad;
        if (!in) {
            std::abort();
        }
        Core_Gte_Reset();
        Core_Gte_SetRotMatrix(&matrix);
        Core_Gte_SetTransMatrix(&matrix);
        Core_Gte_SetGeomOffset(ofx, ofy);
        Core_Gte_SetGeomScreen(h);
        Core_Gte_SetDepthCue(dqa, dqb);
        SVECTOR point { static_cast<short>(x), static_cast<short>(y),
            static_cast<short>(z), static_cast<short>(pad) };
        long sxy = 0, cue = 0, flag = 0, mac0 = 0;
        const long depth = Core_Gte_RotTransPers(&point, &sxy, &cue, &flag);
        Core_Gte_StoreOpz(&mac0);
        std::printf("%ld %ld %ld %ld %ld %d\n", depth, sxy, cue, flag, mac0, point.pad);
    }
}
