#include <SDL.h>
#include <SDL_image.h>
#include <algorithm>
#include <iostream>
#include <vector>
#include <deque>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>

#ifdef USE_CONNEXT
#include <ndds/ndds_cpp.h>
#include "ShipThreat.h"
#include "ShipThreatSupport.h"
#include "threat_qos.h"
#endif

#ifndef USE_CONNEXT
namespace ship {
    struct Threat { int id; double x,y,heading,speed; int severity; };
    struct SensorDetection { int sensor_id; int threat_id; double x,y; int confidence; };
    struct EffectorAction { int effector_id; int threat_id; bool destroyed; double x,y; };
}
#endif

using namespace std::chrono_literals;

// ============================================================
// Embedded 5x7 bitmap font (Adafruit GFX glcdfont, BSD licensed)
// ASCII 32-126.  Each entry: 5 column bytes.  Bit 0 = top row.
// ============================================================
static const uint8_t font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '\''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x08,0x2A,0x1C,0x2A,0x08}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x00,0x08,0x14,0x22,0x41}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x41,0x22,0x14,0x08,0x00}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x01,0x01}, // 'F'
    {0x3E,0x41,0x41,0x51,0x32}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x04,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x7F,0x20,0x18,0x20,0x7F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x03,0x04,0x78,0x04,0x03}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x00,0x7F,0x41,0x41}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x41,0x41,0x7F,0x00,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x08,0x54,0x54,0x54,0x3C}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 'j'
    {0x00,0x7F,0x10,0x28,0x44}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
    {0x00,0x08,0x36,0x41,0x00}, // '{'
    {0x00,0x00,0x7F,0x00,0x00}, // '|'
    {0x00,0x41,0x36,0x08,0x00}, // '}'
    {0x08,0x08,0x2A,0x1C,0x08}, // '~'
};

static void draw_char(SDL_Renderer* ren, int x, int y, char c, int scale = 1) {
    if (c < 32 || c > 126) return;
    const uint8_t* g = font5x7[(unsigned char)c - 32];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (g[col] & (1 << row)) {
                SDL_Rect px = {x + col * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(ren, &px);
            }
        }
    }
}

static void draw_text(SDL_Renderer* ren, int x, int y, const char* s, int scale = 1) {
    for (int i = 0; s[i]; i++)
        draw_char(ren, x + i * 6 * scale, y, s[i], scale);
}

// 9-character fixed-width name lookups for Aegis sensor/weapon systems
static const char* sensor_name(int id) {
    switch (id) {
        case 1: return "AN/SPY-1D";   // Primary Aegis phased-array radar
        case 2: return "AN/SPQ-9B";   // Horizon Search Radar
        case 3: return "AN/SPS-67";   // Surface Search Radar
        case 4: return "AN/SLQ-32";   // Electronic Warfare System
        default: return "SENSOR   ";
    }
}
static const char* effector_name(int id) {
    switch (id) {
        case 1: return "SM-2 MR  ";   // Standard Missile 2 Medium Range
        case 2: return "SM-6     ";   // Standard Missile 6
        case 3: return "ESSM     ";   // Evolved Sea Sparrow Missile
        case 4: return "CIWS     ";   // Phalanx Close-In Weapon System
        case 5: return "MK 45/62 ";   // 5-inch/62 cal gun
        default: return "EFFECTOR ";
    }
}

// ============================================================
// Ship constants, visual-effect structs, and draw helpers
// ============================================================
static const int SHIP_X = 400;
static const int SHIP_Y = 570;  // waterline y position (side view)

struct EffectorMount {
    int id;
    SDL_Point position;
    SDL_Color color;
};

static EffectorMount effector_mounts[] = {
    {1, {SHIP_X + 38, SHIP_Y - 37}, {100, 220, 255, 255}},
    {2, {SHIP_X + 81, SHIP_Y - 37}, {100, 255, 160, 255}},
    {3, {SHIP_X - 45, SHIP_Y - 31}, {255, 205, 85, 255}},
    {4, {SHIP_X - 99, SHIP_Y - 69}, {255, 170, 50, 255}},
    {5, {SHIP_X + 143, SHIP_Y - 38}, {215, 220, 230, 255}}
};

static SDL_Point effector_muzzle(const EffectorMount& mount) {
    switch (mount.id) {
        case 1: return {mount.position.x + 9, mount.position.y - 19};
        case 2: return {mount.position.x + 9, mount.position.y - 23};
        case 3: return {mount.position.x + 9, mount.position.y - 14};
        case 4: return {mount.position.x - 10, mount.position.y - 13};
        default: return {mount.position.x + 9, mount.position.y - 10};
    }
}

static SDL_Rect effector_bounds(const EffectorMount& mount) {
    const int x = mount.position.x, y = mount.position.y;
    switch (mount.id) {
        case 1: return {x - 17, y - 23, 29, 27};
        case 2: return {x - 20, y - 27, 33, 31};
        case 3: return {x - 13, y - 26, 25, 30};
        case 4: return {x - 17, y - 21, 28, 21};
        default: return {x - 13, y - 12, 25, 16};
    }
}

static void draw_effector_mounts(SDL_Renderer* ren, SDL_Texture* phalanx_tex) {
    for (const auto& mount : effector_mounts) {
        const int x = mount.position.x, y = mount.position.y;
        if (mount.id == 4 && phalanx_tex) {
            SDL_Rect bounds = effector_bounds(mount);
            SDL_RenderCopy(ren, phalanx_tex, nullptr, &bounds);
            continue;
        }
        const SDL_Point muzzle = effector_muzzle(mount);
        auto line = [&](int from_x, int from_y, int to_x, int to_y) {
            SDL_RenderDrawLine(ren, x + from_x, y + from_y, x + to_x, y + to_y);
        };
        SDL_SetRenderDrawColor(ren, 50, 65, 80, 255);
        SDL_Rect base = {x - 11, y, 22, 4};
        SDL_RenderFillRect(ren, &base);
        SDL_SetRenderDrawColor(ren, 160, 190, 205, 255);
        SDL_Rect pedestal = {x - 4, y - 9, 8, 9};
        SDL_RenderFillRect(ren, &pedestal);
        SDL_SetRenderDrawColor(ren, mount.color.r, mount.color.g, mount.color.b, 255);
        SDL_RenderDrawRect(ren, &base);
        if (mount.id <= 2) {
            const int tail_x = mount.id == 1 ? -13 : -16;
            const float axis_x = float(muzzle.x - (x + tail_x));
            const float axis_y = float(muzzle.y - (y - 8));
            const float length = sqrtf(axis_x * axis_x + axis_y * axis_y);
            auto point = [&](float forward, float side) -> SDL_Point {
                return {int(muzzle.x + (axis_x * forward - axis_y * side) / length),
                        int(muzzle.y + (axis_y * forward + axis_x * side) / length)};
            };
            for (int stripe = -2; stripe <= 2; ++stripe) {
                const auto tail = point(-length, float(stripe));
                const auto nose = point(-6.0f, float(stripe));
                SDL_RenderDrawLine(ren, tail.x, tail.y, nose.x, nose.y);
                SDL_RenderDrawLine(ren, nose.x, nose.y, muzzle.x, muzzle.y);
            }
            const auto fin = point(-length + 3, 6);
            const auto root = point(-length + 9, 2);
            const auto tail = point(-length, 2);
            SDL_RenderDrawLine(ren, tail.x, tail.y, fin.x, fin.y);
            SDL_RenderDrawLine(ren, fin.x, fin.y, root.x, root.y);
            SDL_SetRenderDrawColor(ren, 35, 90, 130, 255);
            const auto band_top = point(-10, -2);
            const auto band_bottom = point(-10, 2);
            SDL_RenderDrawLine(ren, band_top.x, band_top.y, band_bottom.x, band_bottom.y);
            line(-3, -6, 4, -6);
        } else if (mount.id == 3) {
            for (int tube = 0; tube < 3; ++tube) {
                const int offset = tube * 4;
                line(-11, -5 - offset, 9, -14 - offset);
                line(-11, -8 - offset, 9, -17 - offset);
                line(-11, -5 - offset, -11, -8 - offset);
                line(9, -14 - offset, 9, -17 - offset);
            }
        } else if (mount.id == 4) {
            SDL_Rect housing = {x - 10, y - 11, 11, 10};
            SDL_RenderFillRect(ren, &housing);
            SDL_SetRenderDrawColor(ren, 225, 235, 240, 255);
            for (int dome_y = -21; dome_y <= -12; ++dome_y) {
                const int half_width = dome_y < -18 ? 3 : 5;
                line(-5 - half_width, dome_y, -5 + half_width, dome_y);
            }
            SDL_SetRenderDrawColor(ren, mount.color.r, mount.color.g, mount.color.b, 255);
            for (int barrel = 0; barrel < 3; ++barrel)
                line(-1, -10 + barrel, -10, -14 + barrel);
        } else {
            for (int armor_y = -10; armor_y < 0; ++armor_y)
                line(armor_y < -6 ? -8 : -11, armor_y, 0, armor_y);
            line(-1, -6, 9, -10);
            line(-1, -5, 9, -9);
        }
    }
}

struct LaunchPlume { float x, y, age, life; };
struct KillBlast   { float x, y, age, life; };
struct Interceptor { float x, y; int target_id; float speed, age, life; bool will_kill, done; };

// Draw typed threat shape; angle = atan2 direction of travel toward ship
static void draw_threat(SDL_Renderer* ren, int x, int y, float angle, int type, Uint8 alpha = 255) {
    float ca = cosf(angle), sa = sinf(angle);
    auto pt = [&](float fwd, float rt) -> SDL_Point {
        return { x + int(ca*fwd - sa*rt), y + int(sa*fwd + ca*rt) };
    };
    if (type == 0) {
        // Ballistic missile: elongated body + tail fins
        SDL_SetRenderDrawColor(ren, 255, 80, 30, alpha);
        SDL_Point tip  = pt( 9, 0);
        SDL_Point body = pt(-5, 0);
        SDL_Point fl   = pt(-5, 5);
        SDL_Point fr   = pt(-5,-5);
        SDL_Point n2   = pt( 7, 2), n3 = pt( 7,-2);
        SDL_RenderDrawLine(ren, tip.x,  tip.y,  body.x, body.y);
        SDL_RenderDrawLine(ren, body.x, body.y, fl.x,   fl.y  );
        SDL_RenderDrawLine(ren, body.x, body.y, fr.x,   fr.y  );
        SDL_RenderDrawLine(ren, n2.x,   n2.y,   tip.x,  tip.y );
        SDL_RenderDrawLine(ren, n3.x,   n3.y,   tip.x,  tip.y );
    } else if (type == 1) {
        // Drone / UAS: X-frame with rotor blobs
        SDL_SetRenderDrawColor(ren, 180, 40, 210, alpha);
        for (int arm = 0; arm < 4; arm++) {
            float aa = arm * 3.14159f / 2.0f + 3.14159f / 4.0f;
            SDL_Point tip2 = { x + int(cosf(aa)*8), y + int(sinf(aa)*8) };
            SDL_RenderDrawLine(ren, x, y, tip2.x, tip2.y);
            SDL_Rect dot = { tip2.x - 2, tip2.y - 2, 4, 4 };
            SDL_RenderFillRect(ren, &dot);
        }
        SDL_Rect body2 = { x - 2, y - 2, 4, 4 };
        SDL_RenderFillRect(ren, &body2);
    } else {
        // Anti-ship cruise missile: swept delta wing
        SDL_SetRenderDrawColor(ren, 255, 50, 50, alpha);
        SDL_Point tip  = pt( 9, 0);
        SDL_Point wl   = pt(-5, 8);
        SDL_Point wr   = pt(-5,-8);
        SDL_Point tail = pt(-7, 0);
        SDL_RenderDrawLine(ren, tip.x, tip.y, wl.x, wl.y);
        SDL_RenderDrawLine(ren, tip.x, tip.y, wr.x, wr.y);
        SDL_RenderDrawLine(ren, wl.x, wl.y, tail.x, tail.y);
        SDL_RenderDrawLine(ren, wr.x, wr.y, tail.x, tail.y);
    }
}

// Draw Arleigh Burke-class destroyer side profile
// Bow faces RIGHT, stern faces LEFT  |  cx = hull centre-x, cy = waterline
static void draw_destroyer(SDL_Renderer* ren, int cx, int cy) {
    const int SL  = cx - 118;  // stern left  edge
    const int BR  = cx + 118;  // bow   right edge  (hull body ends here)
    const int DK  = cy - 16;   // main deck
    const int FDK = DK - 6;    // raised forecastle
    const int BK  = cy + 9;    // keel

    // -- below-waterline hull (dark charcoal) --
    SDL_SetRenderDrawColor(ren, 50, 58, 66, 255);
    for (int iy = cy; iy <= BK; iy++) {
        float t  = float(iy - cy) / float(BK - cy);
        int   lx = SL + 6 + int(4 * t);
        int   rx = BR - 4 - int(4 * t);
        SDL_RenderDrawLine(ren, lx, iy, rx, iy);
    }

    // -- main hull above waterline (light grey) --
    SDL_SetRenderDrawColor(ren, 128, 138, 148, 255);
    SDL_Rect hull = {SL + 4, DK, BR - SL - 12, cy - DK};
    SDL_RenderFillRect(ren, &hull);

    // -- raked bow (scan-fill triangle, bow on right) --
    for (int iy = DK; iy <= BK; iy++) {
        float t  = float(iy - DK) / float(BK - DK);
        // peak at waterline, taper top and bottom
        int tip = BR - 2 + int(14.0f * sinf(t * 3.14159f));
        SDL_RenderDrawLine(ren, BR - 12, iy, tip, iy);
    }

    // -- stern (vertical with slight rake) --
    SDL_Rect stern_fill = {SL, DK, 8, BK - DK};
    SDL_RenderFillRect(ren, &stern_fill);

    // -- prominent tan/khaki weather-deck stripe (visible in real ship photos) --
    SDL_SetRenderDrawColor(ren, 162, 148, 98, 255);
    SDL_Rect deck_stripe = {SL + 4, DK, BR - SL - 12, 3};
    SDL_RenderFillRect(ren, &deck_stripe);

    // -- raised forecastle (forward, right half of ship) --
    SDL_SetRenderDrawColor(ren, 120, 130, 140, 255);
    SDL_Rect fc = {cx + 8, FDK, BR - cx - 20, DK - FDK};
    SDL_RenderFillRect(ren, &fc);
    SDL_SetRenderDrawColor(ren, 155, 142, 94, 255);
    SDL_Rect fc_stripe = {cx + 8, FDK, BR - cx - 20, 2};
    SDL_RenderFillRect(ren, &fc_stripe);

    // -- Mk45 5"/62 gun mount (on forecastle, near bow) --
    SDL_SetRenderDrawColor(ren, 105, 112, 122, 255);
    SDL_Rect gun_base = {cx + 80, FDK - 8, 18, 8};
    SDL_RenderFillRect(ren, &gun_base);
    SDL_SetRenderDrawColor(ren, 88, 95, 104, 255);
    SDL_RenderDrawLine(ren, cx + 80, FDK - 5, cx + 102, FDK - 5);
    SDL_RenderDrawLine(ren, cx + 80, FDK - 4, cx + 102, FDK - 4);

    // -- VLS forward cells (Mark 41, forecastle) --
    SDL_SetRenderDrawColor(ren, 110, 118, 128, 255);
    SDL_Rect vls_f = {cx + 20, FDK - 4, 50, 4};
    SDL_RenderFillRect(ren, &vls_f);
    SDL_SetRenderDrawColor(ren, 85, 92, 102, 255);
    for (int v = 1; v < 6; v++)
        SDL_RenderDrawLine(ren, cx+20+v*8, FDK-4, cx+20+v*8, FDK);

    // -- forward superstructure base (wide, at main-deck level) --
    SDL_SetRenderDrawColor(ren, 142, 152, 162, 255);
    SDL_Rect fwd_lo = {cx - 18, FDK, 38, DK - FDK};
    SDL_RenderFillRect(ren, &fwd_lo);
    SDL_Rect fwd_mid = {cx - 14, DK - 32, 32, 32 - (DK - FDK)};
    SDL_RenderFillRect(ren, &fwd_mid);

    // -- bridge level (stepped inward) --
    SDL_SetRenderDrawColor(ren, 150, 160, 170, 255);
    SDL_Rect bridge = {cx - 10, DK - 44, 26, 13};
    SDL_RenderFillRect(ren, &bridge);
    // bridge windows
    SDL_SetRenderDrawColor(ren, 45, 68, 95, 255);
    SDL_Rect wins = {cx - 9, DK - 43, 24, 4};
    SDL_RenderFillRect(ren, &wins);

    // -- SPY-1D phased array radar panel (flat rectangle, on mast face) --
    SDL_SetRenderDrawColor(ren, 118, 126, 136, 255);
    SDL_Rect spy = {cx - 12, DK - 57, 22, 15};
    SDL_RenderFillRect(ren, &spy);
    // panel divider lines
    SDL_SetRenderDrawColor(ren, 95, 102, 112, 255);
    SDL_RenderDrawLine(ren, cx - 1, DK - 57, cx - 1, DK - 42);
    SDL_RenderDrawLine(ren, cx - 12, DK - 49, cx + 10, DK - 49);

    // -- foremast (twin poles + yardarms + stays) --
    SDL_SetRenderDrawColor(ren, 168, 174, 182, 255);
    SDL_RenderDrawLine(ren, cx - 5, DK - 44, cx - 5, DK - 90);
    SDL_RenderDrawLine(ren, cx - 2, DK - 44, cx - 2, DK - 82);
    // top sensor pod
    SDL_SetRenderDrawColor(ren, 148, 154, 162, 255);
    SDL_Rect mast_top = {cx - 10, DK - 96, 14, 8};
    SDL_RenderFillRect(ren, &mast_top);
    // yardarms
    SDL_SetRenderDrawColor(ren, 158, 164, 172, 255);
    SDL_RenderDrawLine(ren, cx - 20, DK - 74, cx + 14, DK - 74);
    SDL_RenderDrawLine(ren, cx - 17, DK - 64, cx + 11, DK - 64);
    // antenna stays
    SDL_SetRenderDrawColor(ren, 128, 134, 142, 255);
    SDL_RenderDrawLine(ren, cx - 20, DK - 74, cx - 5, DK - 90);
    SDL_RenderDrawLine(ren, cx + 14, DK - 74, cx - 2, DK - 82);

    // -- funnel / exhaust uptake --
    SDL_SetRenderDrawColor(ren, 106, 114, 122, 255);
    SDL_Rect funnelb = {cx - 48, FDK, 18, DK - FDK + 20};
    SDL_RenderFillRect(ren, &funnelb);
    SDL_SetRenderDrawColor(ren, 88, 95, 104, 255);
    SDL_Rect funnel_top_rect = {cx - 50, FDK - 6, 22, 7};
    SDL_RenderFillRect(ren, &funnel_top_rect);
    SDL_SetRenderDrawColor(ren, 32, 36, 40, 255);
    SDL_Rect funnel_opening = {cx - 49, FDK - 5, 20, 4};
    SDL_RenderFillRect(ren, &funnel_opening);

    // -- aft superstructure --
    SDL_SetRenderDrawColor(ren, 136, 146, 156, 255);
    SDL_Rect aft_lo = {cx - 78, FDK, 26, DK - FDK};
    SDL_RenderFillRect(ren, &aft_lo);
    SDL_Rect aft_hi = {cx - 76, DK - 26, 22, 26 - (DK - FDK)};
    SDL_RenderFillRect(ren, &aft_hi);
    SDL_Rect aft_top_rect = {cx - 74, DK - 32, 18, 7};
    SDL_RenderFillRect(ren, &aft_top_rect);

    // -- aft mast + sensor --
    SDL_SetRenderDrawColor(ren, 158, 164, 172, 255);
    SDL_RenderDrawLine(ren, cx - 66, DK - 32, cx - 66, DK - 56);
    SDL_RenderDrawLine(ren, cx - 74, DK - 48, cx - 58, DK - 48);
    SDL_RenderDrawLine(ren, cx - 72, DK - 40, cx - 60, DK - 40);

    // -- VLS aft cells (aft of bridge, on main deck) --
    SDL_SetRenderDrawColor(ren, 108, 116, 126, 255);
    SDL_Rect vls_a = {cx - 15, DK - 3, 28, 3};
    SDL_RenderFillRect(ren, &vls_a);
    SDL_SetRenderDrawColor(ren, 82, 89, 99, 255);
    for (int v = 1; v < 4; v++)
        SDL_RenderDrawLine(ren, cx-15+v*7, DK-3, cx-15+v*7, DK);

    // -- helicopter deck (stern, flat, with H marking) --
    SDL_SetRenderDrawColor(ren, 112, 122, 132, 255);
    SDL_Rect helo = {SL + 6, FDK, cx - 79 - SL - 6, DK - FDK};
    SDL_RenderFillRect(ren, &helo);
    // H-marking
    SDL_SetRenderDrawColor(ren, 80, 90, 100, 255);
    int hx = SL + 6 + (cx - 79 - SL - 6) / 2;
    SDL_RenderDrawLine(ren, hx - 7, FDK + 3, hx - 7, FDK + 10);
    SDL_RenderDrawLine(ren, hx + 7, FDK + 3, hx + 7, FDK + 10);
    SDL_RenderDrawLine(ren, hx - 7, FDK + 6, hx + 7, FDK + 6);

    // -- hull outline --
    SDL_SetRenderDrawColor(ren, 65, 75, 84, 255);
    SDL_RenderDrawLine(ren, SL + 4, DK,     BR - 12, DK);      // main deck
    SDL_RenderDrawLine(ren, BR - 12, DK,    BR + 12, cy - 5);  // bow slope
    SDL_RenderDrawLine(ren, BR + 12, cy-5,  BR + 10, BK - 2);  // bow face
    SDL_RenderDrawLine(ren, BR + 10, BK-2,  BR - 12, BK);      // bow keel
    SDL_RenderDrawLine(ren, BR - 12, BK,    SL + 6,  BK);      // keel
    SDL_RenderDrawLine(ren, SL + 4,  DK,    SL + 4,  BK + 1);  // stern

    // -- wake lines astern --
    SDL_SetRenderDrawColor(ren, 28, 72, 120, 200);
    SDL_RenderDrawLine(ren, SL + 4, cy,     SL - 10, cy + 3);
    SDL_RenderDrawLine(ren, SL + 4, cy + 2, SL - 18, cy + 5);
    SDL_RenderDrawLine(ren, SL + 4, cy - 1, SL - 14, cy + 2);
}

// Launch plume: expanding stippled smoke cloud at bow
static void draw_plume(SDL_Renderer* ren, const LaunchPlume& p) {
    float frac = p.age / p.life;
    float r = 4.0f + 18.0f * frac;
    Uint8 bright = Uint8(230.0f * (1.0f - frac));
    SDL_SetRenderDrawColor(ren, bright, bright, bright, 255);
    int ri = int(r);
    for (int dy = -ri; dy <= ri; dy++) {
        for (int dx = -ri; dx <= ri; dx++) {
            if (dx*dx + dy*dy <= ri*ri && ((dx + dy) & 1) == 0)
                SDL_RenderDrawPoint(ren, int(p.x)+dx, int(p.y)+dy);
        }
    }
}

// Draw interceptor missile (small bright triangle + exhaust glow)
static void draw_interceptor(SDL_Renderer* ren, float x, float y, float angle) {
    float ca = cosf(angle), sa = sinf(angle);
    auto pt = [&](float fwd, float rt) -> SDL_Point {
        return { int(x + ca*fwd - sa*rt), int(y + sa*fwd + ca*rt) };
    };
    SDL_SetRenderDrawColor(ren, 200, 255, 200, 255);
    SDL_Point tip = pt(6,0), bl = pt(-3,3), br = pt(-3,-3);
    SDL_RenderDrawLine(ren, tip.x, tip.y, bl.x,  bl.y );
    SDL_RenderDrawLine(ren, tip.x, tip.y, br.x,  br.y );
    SDL_RenderDrawLine(ren, bl.x,  bl.y,  br.x,  br.y );
    SDL_SetRenderDrawColor(ren, 255, 220, 80, 255);
    SDL_Point e1 = pt(-4,0), e2 = pt(-9,0);
    SDL_RenderDrawLine(ren, e1.x, e1.y, e2.x, e2.y);
}

// Kill blast: expanding ring + radial sparks + central flash
static void draw_blast(SDL_Renderer* ren, const KillBlast& b) {
    float frac = b.age / b.life;
    float r = 4.0f + 24.0f * frac;
    // Central flash (white-yellow, fades quickly)
    if (frac < 0.35f) {
        float fr = 8.0f * (1.0f - frac / 0.35f);
        SDL_SetRenderDrawColor(ren, 255, 255, 210, 255);
        SDL_Rect fl = {int(b.x - fr), int(b.y - fr), int(2*fr+1), int(2*fr+1)};
        SDL_RenderFillRect(ren, &fl);
    }
    // Expanding orange ring
    SDL_SetRenderDrawColor(ren, 255, Uint8(160.0f * (1.0f - frac*0.5f)), 0, 255);
    for (int deg = 0; deg < 360; deg += 4) {
        float a = deg * 3.14159f / 180.0f;
        SDL_RenderDrawPoint(ren, int(b.x + r     * cosf(a)), int(b.y + r     * sinf(a)));
        SDL_RenderDrawPoint(ren, int(b.x + (r+1) * cosf(a)), int(b.y + (r+1) * sinf(a)));
    }
    // Radial sparks
    SDL_SetRenderDrawColor(ren, 255, 220, 50, 255);
    for (int s = 0; s < 8; s++) {
        float a = s * 3.14159f / 4.0f;
        SDL_RenderDrawLine(ren,
            int(b.x + r*0.6f * cosf(a)), int(b.y + r*0.6f * sinf(a)),
            int(b.x + (r+5)  * cosf(a)), int(b.y + (r+5)  * sinf(a)));
    }
}

#ifdef USE_CONNEXT
// DDS globals
static ship::ThreatDataWriter *g_threat_writer = nullptr;
static std::map<int, std::unique_ptr<ThreatPublisher>> g_configured_publishers;

class SensorDetectionListener : public DDSDataReaderListener {
public:
    std::map<int, ship::SensorDetection> latest;      // keyed by sensor_id
    std::map<int, std::chrono::steady_clock::time_point> last_seen;
    std::mutex mtx;
    void on_data_available(DDSDataReader* reader) override {
        ship::SensorDetectionDataReader *sr = ship::SensorDetectionDataReader::narrow(reader);
        if (!sr) return;
        ship::SensorDetectionSeq seq;
        DDS_SampleInfoSeq infos;
        DDS_ReturnCode_t ret = sr->take(seq, infos, DDS_LENGTH_UNLIMITED,
                                        DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
        if (ret == DDS_RETCODE_OK) {
            std::lock_guard<std::mutex> lk(mtx);
            for (DDS_Long i = 0; i < seq.length(); ++i) {
                latest[seq[i].sensor_id]    = seq[i];
                last_seen[seq[i].sensor_id] = std::chrono::steady_clock::now();
            }
        }
        if (ret == DDS_RETCODE_OK)
            sr->return_loan(seq, infos);
    }
};

// Effect event queued by listener for main thread to consume
struct EffectEvent { float lx, ly; int target_id; int effector_id; bool will_kill; };

class EffectorActionListener : public DDSDataReaderListener {
public:
    std::map<int, ship::EffectorAction> latest;
    std::map<int, std::chrono::steady_clock::time_point> last_seen;
    std::vector<EffectEvent> pending_fx;   // drained each frame by main loop
    std::mutex mtx;
    void on_data_available(DDSDataReader* reader) override {
        ship::EffectorActionDataReader *sr = ship::EffectorActionDataReader::narrow(reader);
        if (!sr) return;
        ship::EffectorActionSeq seq;
        DDS_SampleInfoSeq infos;
        DDS_ReturnCode_t ret = sr->take(seq, infos, DDS_LENGTH_UNLIMITED,
                                        DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
        if (ret == DDS_RETCODE_OK) {
            std::lock_guard<std::mutex> lk(mtx);
            for (DDS_Long i = 0; i < seq.length(); ++i) {
                latest[seq[i].effector_id]    = seq[i];
                last_seen[seq[i].effector_id] = std::chrono::steady_clock::now();
                const EffectorMount* mount = nullptr;
                for (const auto& candidate : effector_mounts)
                    if (candidate.id == seq[i].effector_id) mount = &candidate;
                if (!mount) continue;
                const auto muzzle = effector_muzzle(*mount);
                pending_fx.push_back({
                    float(muzzle.x), float(muzzle.y),
                    int(seq[i].threat_id),
                    int(seq[i].effector_id),
                    seq[i].destroyed != DDS_BOOLEAN_FALSE
                });
            }
        }
        if (ret == DDS_RETCODE_OK)
            sr->return_loan(seq, infos);
    }
};

#endif

#ifdef USE_CONNEXT
static SensorDetectionListener sensor_listener;
static EffectorActionListener effector_listener;
#endif

void publishThreat(const ship::Threat &t, std::deque<ship::Threat>& history)
{
#ifdef USE_CONNEXT
    const auto configured = g_configured_publishers.find(t.id);
    auto* writer = configured == g_configured_publishers.end()
        ? g_threat_writer : configured->second->writer;
    if (writer) {
        ship::Threat sample = t;
        DDS_ReturnCode_t ret = writer->write(sample, DDS_HANDLE_NIL);
        if (ret != DDS_RETCODE_OK) {
            std::cerr << "Threat write failed: " << ret << std::endl;
            return;
        }
    } else
#endif
    {
        std::cout << "[PUB] Threat id=" << t.id << " x=" << t.x << " y=" << t.y << "\n";
    }
    history.push_back(t);
    if (history.size() > 5) history.pop_front();
}

#ifdef USE_CONNEXT
static int check_effector_qos()
{
    auto* factory = DDSDomainParticipantFactory::get_instance();
    auto* participant = factory->create_participant(
        0, DDS_PARTICIPANT_QOS_DEFAULT, nullptr, DDS_STATUS_MASK_NONE);
    if (!participant) return 1;
    bool passed = false;
    {
        effector_qos::Channel channel;
        effector_qos::Settings originals[5];
        bool changed = false;
        std::string error;
        auto snapshot = [](const effector_qos::Settings& settings) {
            std::ostringstream output;
            effector_qos::encode(output, settings);
            return output.str();
        };
        passed = [&]() {
            if (!channel.initialize(participant, false)) return false;
            for (int id = 1; id <= 5; ++id) {
                participant->get_default_publisher_qos(originals[id - 1].publisher);
                participant->get_default_datawriter_qos(originals[id - 1].writer);
                if (!channel.exchange("GET", id, originals[id - 1], error)) return false;
            }
            auto requested = originals[3];
            requested.writer.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
            requested.writer.history.depth = 3;
            if (!channel.exchange("SET", 4, requested, error)) return false;
            changed = true;
            if (!channel.exchange("GET", 4, requested, error) || requested.writer.history.depth != 3) {
                error = "CIWS history change did not persist.";
                return false;
            }
            for (int id : {1, 2, 3, 5}) {
                auto other = originals[id - 1];
                if (!channel.exchange("GET", id, other, error)) return false;
                if (snapshot(other) != snapshot(originals[id - 1])) {
                    error = "Changing CIWS also changed another effector.";
                    return false;
                }
            }
            requested.writer.history.depth = 0;
            if (channel.exchange("SET", 4, requested, error)) {
                error = "Invalid history depth was accepted.";
                return false;
            }
            error.clear();
            if (!channel.exchange("GET", 4, requested, error) || requested.writer.history.depth != 3) {
                error = "A rejected update changed the live effector settings.";
                return false;
            }
            return true;
        }();
        if (changed && !channel.exchange("SET", 4, originals[3], error)) passed = false;
        if (!passed) std::cerr << "Effector QoS check failed: " << error << '\n';
        else std::cout << "Effector QoS check passed: GET/SET, persistence, isolation, rejection, restoration.\n";
    }
    participant->delete_contained_entities();
    factory->delete_participant(participant);
    return passed ? 0 : 1;
}

static int check_reader_qos()
{
    auto* factory = DDSDomainParticipantFactory::get_instance();
    auto* participant = factory->create_participant(
        0, DDS_PARTICIPANT_QOS_DEFAULT, nullptr, DDS_STATUS_MASK_NONE);
    if (!participant) return 1;
    bool passed = false;
    std::string error;
    {
        effector_qos::Channel sensor_control, effector_control;
        auto snapshot = [](const effector_qos::ReaderSettings& settings) {
            std::ostringstream output;
            effector_qos::encode(output, settings);
            return output.str();
        };
        auto check_remote = [&](effector_qos::Channel& control) {
            effector_qos::ReaderSettings original;
            participant->get_default_subscriber_qos(original.subscriber);
            participant->get_default_datareader_qos(original.reader);
            if (!control.exchange("GET_READER", 0, original, error)) return false;
            auto requested = original;
            requested.reader.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
            requested.reader.history.depth = 3;
            requested.reader.deadline.period = DDS_DURATION_INFINITE;
            requested.reader.time_based_filter.minimum_separation = {0, 100000000};
            requested.subscriber.partition.name.ensure_length(1, 1);
            DDS_String_free(requested.subscriber.partition.name[0]);
            requested.subscriber.partition.name[0] = DDS_String_dup("Reader QoS check partition");
            const auto expected = snapshot(requested);
            if (!control.exchange("SET_READER", 0, requested, error)) return false;
            bool valid = control.exchange("GET_READER", 0, requested, error) && snapshot(requested) == expected;
            if (valid) {
                auto invalid = requested;
                invalid.reader.history.depth = 0;
                valid = !control.exchange("SET_READER", 0, invalid, error);
                invalid = requested;
                invalid.reader.deadline.period = {0, 50000000};
                valid = valid && !control.exchange("SET_READER", 0, invalid, error);
                error.clear();
                valid = valid && control.exchange("GET_READER", 0, requested, error) && snapshot(requested) == expected;
            }
            if (!control.exchange("SET_READER", 0, original, error)) return false;
            if (!valid && error.empty()) error = "Remote reader readback or rejection check failed.";
            return valid;
        };
        passed = [&]() {
            if (!sensor_control.initialize(participant, false, "Sensor") ||
                !effector_control.initialize(participant, false)) {
                error = "Cannot initialize reader QoS check channels.";
                return false;
            }
            if (!check_remote(sensor_control) || !check_remote(effector_control)) return false;
            ship::EffectorActionTypeSupport::register_type(participant, ship::EffectorActionTypeSupport::get_type_name());
            auto* topic = participant->create_topic("ReaderQosCheckActions",
                ship::EffectorActionTypeSupport::get_type_name(), DDS_TOPIC_QOS_DEFAULT, nullptr, DDS_STATUS_MASK_NONE);
            effector_qos::ReaderSettings defaults;
            participant->get_default_subscriber_qos(defaults.subscriber);
            participant->get_default_datareader_qos(defaults.reader);
            defaults.reader.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
            auto create_filtered = [&](int id) {
                DDS_StringSeq parameters;
                parameters.ensure_length(1, 1);
                parameters[0] = DDS_String_dup(std::to_string(id).c_str());
                const auto name = "ReaderQosCheckFilter" + std::to_string(id);
                auto* filtered = participant->create_contentfilteredtopic(
                    name.c_str(), topic, "effector_id = %0", parameters);
                return filtered ? effector_qos::create_reader(participant, filtered, nullptr,
                    defaults, topic->get_name()) : nullptr;
            };
            auto sm2_reader = create_filtered(1);
            auto ciws_reader = create_filtered(4);
            if (!sm2_reader || !ciws_reader) { error = "Cannot create filtered action readers."; return false; }
            const auto sm2_original = snapshot(sm2_reader->settings);
            auto changed = ciws_reader->settings;
            changed.reader.history.depth = 3;
            if (!effector_qos::replace_reader(ciws_reader, changed, error)) return false;
            if (snapshot(sm2_reader->settings) != sm2_original || ciws_reader->settings.reader.history.depth != 3) {
                error = "Action reader change was not isolated.";
                return false;
            }
            auto* publisher = participant->create_publisher(DDS_PUBLISHER_QOS_DEFAULT, nullptr, DDS_STATUS_MASK_NONE);
            DDS_DataWriterQos writer_qos;
            participant->get_default_datawriter_qos(writer_qos);
            writer_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
            auto* writer = publisher ? ship::EffectorActionDataWriter::narrow(publisher->create_datawriter(
                topic, writer_qos, nullptr, DDS_STATUS_MASK_NONE)) : nullptr;
            if (!writer) { error = "Cannot create action check writer."; return false; }
            const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            bool sent = false, received_sm2 = false, received_ciws = false;
            while (std::chrono::steady_clock::now() < until && !(received_sm2 && received_ciws)) {
                DDS_PublicationMatchedStatus matched;
                if (!sent && writer->get_publication_matched_status(matched) == DDS_RETCODE_OK && matched.current_count == 2) {
                    ship::EffectorAction sample{};
                    sample.effector_id = 1;
                    sample.threat_id = 900001;
                    writer->write(sample, DDS_HANDLE_NIL);
                    sample.effector_id = 4;
                    writer->write(sample, DDS_HANDLE_NIL);
                    sent = true;
                }
                auto take = [&](effector_qos::ReaderEndpoint& endpoint, int expected, bool& received) {
                    auto* reader = ship::EffectorActionDataReader::narrow(endpoint.reader);
                    ship::EffectorActionSeq samples;
                    DDS_SampleInfoSeq infos;
                    if (reader->take(samples, infos, DDS_LENGTH_UNLIMITED, DDS_ANY_SAMPLE_STATE,
                        DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE) == DDS_RETCODE_OK) {
                        bool valid = true;
                        for (DDS_Long index = 0; index < samples.length(); ++index) {
                            if (!infos[index].valid_data) continue;
                            if (samples[index].effector_id != expected) valid = false;
                            else received = true;
                        }
                        reader->return_loan(samples, infos);
                        return valid;
                    }
                    return true;
                };
                if (!take(*sm2_reader, 1, received_sm2) || !take(*ciws_reader, 4, received_ciws)) {
                    error = "An action reader received the wrong effector ID.";
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (!received_sm2 || !received_ciws) { error = "Filtered action delivery timed out."; return false; }
            return true;
        }();
    }
    participant->delete_contained_entities();
    factory->delete_participant(participant);
    if (passed) std::cout << "Reader QoS check passed: both remote readers, readback, rejection, restoration, isolated action delivery.\n";
    else std::cerr << "Reader QoS check failed: " << error << '\n';
    return passed ? 0 : 1;
}
#endif

int main(int argc, char **argv)
{
    if (argc == 2 && std::string(argv[1]) == "--check-dds") {
#ifdef USE_CONNEXT
        std::cout << "Command & Control: DDS enabled (domain 0).\n";
        return 0;
#else
        std::cerr << "Command & Control: DDS disabled; rebuild with -DUSE_CONNEXT=ON.\n";
        return 1;
#endif
    }
#ifdef USE_CONNEXT
    if (argc == 2 && std::string(argv[1]) == "--check-effector-qos") return check_effector_qos();
    if (argc == 2 && std::string(argv[1]) == "--check-reader-qos") return check_reader_qos();
#endif
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("Ship Defense - Command & Control", 100, 100, 1060, 600, SDL_WINDOW_SHOWN);
    if (!win) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        SDL_DestroyWindow(win);
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Load ship photo texture — resolve path relative to the executable directory
    // so it works regardless of the working directory the user launches from.
    IMG_Init(IMG_INIT_PNG);
    SDL_Texture* ship_tex = nullptr;
    {
        char img_path[2048] = {};
        char* base = SDL_GetBasePath();   // e.g. "C:\...\Debug\"
        if (base) {
            SDL_snprintf(img_path, sizeof(img_path), "%sArleighBurke-class.png", base);
            SDL_free(base);
        } else {
            SDL_strlcpy(img_path, "ArleighBurke-class.png", sizeof(img_path));
        }
        SDL_Surface* surf = IMG_Load(img_path);
        if (surf) {
            ship_tex = SDL_CreateTextureFromSurface(ren, surf);
            SDL_SetTextureBlendMode(ship_tex, SDL_BLENDMODE_BLEND);  // honour PNG alpha
            SDL_FreeSurface(surf);
        } else {
            std::cerr << "IMG_Load (" << img_path << "): " << IMG_GetError() << std::endl;
        }
    }

    SDL_Texture* phalanx_tex = nullptr;
    {
        char img_path[2048] = {};
        char* base = SDL_GetBasePath();
        SDL_snprintf(img_path, sizeof(img_path), "%sStylized Orange Phalanx CIWS Firing.png",
                     base ? base : "");
        SDL_free(base);
        SDL_Surface* original = IMG_Load(img_path);
        if (original) {
            SDL_Surface* surface = SDL_ConvertSurfaceFormat(original, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(original);
            if (surface && SDL_LockSurface(surface) == 0) {
                for (int row = 0; row < surface->h; ++row) {
                    auto* pixels = reinterpret_cast<Uint32*>(
                        static_cast<Uint8*>(surface->pixels) + row * surface->pitch);
                    for (int column = 0; column < surface->w; ++column) {
                        Uint8 red, green, blue, alpha;
                        SDL_GetRGBA(pixels[column], surface->format, &red, &green, &blue, &alpha);
                        const int lightest = (std::max)({int(red), int(green), int(blue)});
                        const int darkest = (std::min)({int(red), int(green), int(blue)});
                        if (darkest >= 230 && lightest - darkest < 20) {
                            alpha = Uint8(int(alpha) * (255 - darkest) / 25);
                            pixels[column] = SDL_MapRGBA(surface->format, red, green, blue, alpha);
                        }
                    }
                }
                SDL_UnlockSurface(surface);
                phalanx_tex = SDL_CreateTextureFromSurface(ren, surface);
                if (phalanx_tex) {
                    SDL_SetTextureBlendMode(phalanx_tex, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureScaleMode(phalanx_tex, SDL_ScaleModeLinear);
                }
            }
            SDL_FreeSurface(surface);
        } else {
            std::cerr << "IMG_Load (" << img_path << "): " << IMG_GetError() << std::endl;
        }
    }

    SDL_Rect ship_bounds = {};
    if (ship_tex) {
        int texture_width = 0, texture_height = 0;
        SDL_QueryTexture(ship_tex, nullptr, nullptr, &texture_width, &texture_height);
        const int display_height = 84;
        const int display_width = texture_height > 0
            ? display_height * texture_width / texture_height : 276;
        ship_bounds = {SHIP_X - display_width / 2, SHIP_Y + 5 - display_height,
                       display_width, display_height};
        const SDL_FPoint locations[] = {
            {0.70f, 0.77f}, {0.82f, 0.77f}, {0.29f, 0.77f},
            {0.42f, 0.59f}, {0.91f, 0.76f}
        };
        for (int index = 0; index < 5; ++index) {
            effector_mounts[index].position = {
                ship_bounds.x + int(locations[index].x * ship_bounds.w),
                ship_bounds.y + int(locations[index].y * ship_bounds.h)
            };
        }
    }

    bool running = true;
    SDL_Event e;
    std::vector<ship::Threat> threats;
    std::map<int, std::deque<ship::Threat>> threat_history;
    std::vector<LaunchPlume>  plumes;
    std::vector<KillBlast>    blasts;
    std::vector<Interceptor>  interceptors;
    int next_id = 1;
    std::mutex data_mtx;

#ifdef USE_CONNEXT
    // DDS setup
    DDSDomainParticipant *participant = DDSDomainParticipantFactory::get_instance()->create_participant(
        0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    DDSTopic* threat_topic = nullptr;
    DDSTopic* effector_topic = nullptr;
    effector_qos::Channel qos_channel;
    effector_qos::Channel sensor_qos_channel;
    std::vector<std::unique_ptr<effector_qos::ReaderEndpoint>> action_readers(5);
    std::vector<ReaderQosTarget> threat_reader_targets;
    if (participant == NULL) {
        std::cerr << "create_participant error" << std::endl;
        return 1;
    } else {
        // register types
        ship::ThreatTypeSupport::register_type(participant, ship::ThreatTypeSupport::get_type_name());
        ship::SensorDetectionTypeSupport::register_type(participant, ship::SensorDetectionTypeSupport::get_type_name());
        ship::EffectorActionTypeSupport::register_type(participant, ship::EffectorActionTypeSupport::get_type_name());

        // create topics
        threat_topic = participant->create_topic("ThreatTopic",
            ship::ThreatTypeSupport::get_type_name(), DDS_TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
        DDSTopic *sensor_topic = participant->create_topic("SensorDetectionTopic",
            ship::SensorDetectionTypeSupport::get_type_name(), DDS_TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
        effector_topic = participant->create_topic("EffectorActionTopic",
            ship::EffectorActionTypeSupport::get_type_name(), DDS_TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
        if (!threat_topic || !sensor_topic || !effector_topic) {
            std::cerr << "Cannot create command-control DDS topics.\n";
            return 1;
        }
        if (!qos_channel.initialize(participant, false)) {
            std::cerr << "Cannot initialize effector QoS control channel.\n";
            return 1;
        }
        if (!sensor_qos_channel.initialize(participant, false, "Sensor")) {
            std::cerr << "Cannot initialize sensor QoS control channel.\n";
            return 1;
        }
        auto reader_target = [participant](const char* name, effector_qos::Channel& control) {
            ReaderQosTarget target;
            target.name = name;
            target.scope = "Shared ThreatTopic reader. The selected QoS is applied during publisher setup.";
            target.topic_name = "ThreatTopic";
            target.get = [participant, channel = &control](effector_qos::ReaderSettings& settings,
                                                          std::string& error) {
                participant->get_default_subscriber_qos(settings.subscriber);
                participant->get_default_datareader_qos(settings.reader);
                return channel->exchange("GET_READER", 0, settings, error);
            };
            target.set = [channel = &control](const effector_qos::ReaderSettings& settings, std::string& error) {
                auto requested = settings;
                return channel->exchange("SET_READER", 0, requested, error);
            };
            return target;
        };
        threat_reader_targets.push_back(reader_target("Sensor Reader", sensor_qos_channel));
        threat_reader_targets.push_back(reader_target("Effector Reader", qos_channel));

        // publisher/writer
        DDSPublisher *publisher = participant->create_publisher(DDS_PUBLISHER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
        if (!publisher) { std::cerr << "Cannot create threat publisher.\n"; return 1; }
        DDSDataWriter *dw = publisher->create_datawriter(threat_topic, DDS_DATAWRITER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
        g_threat_writer = ship::ThreatDataWriter::narrow(dw);
        if (!g_threat_writer) { std::cerr << "Cannot create threat writer.\n"; return 1; }

        // subscriber/listeners
        DDSSubscriber *subscriber = participant->create_subscriber(DDS_SUBSCRIBER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
        if (!subscriber) { std::cerr << "Cannot create sensor subscriber.\n"; return 1; }

        DDSDataReader *sd_dr = subscriber->create_datareader(sensor_topic, DDS_DATAREADER_QOS_DEFAULT, &sensor_listener, DDS_DATA_AVAILABLE_STATUS);
        if (!sd_dr) { std::cerr << "Cannot create sensor detection reader.\n"; return 1; }

        effector_qos::ReaderSettings reader_defaults;
        participant->get_default_subscriber_qos(reader_defaults.subscriber);
        participant->get_default_datareader_qos(reader_defaults.reader);
        for (int id = 1; id <= 5; ++id) {
            DDS_StringSeq parameters;
            parameters.ensure_length(1, 1);
            parameters[0] = DDS_String_dup(std::to_string(id).c_str());
            const auto name = "EffectorActionReader" + std::to_string(id);
            auto* filtered_topic = participant->create_contentfilteredtopic(
                name.c_str(), effector_topic, "effector_id = %0", parameters);
            if (filtered_topic)
                action_readers[id - 1] = effector_qos::create_reader(participant, filtered_topic,
                    &effector_listener, reader_defaults, effector_topic->get_name());
            if (!action_readers[id - 1]) {
                std::cerr << "Cannot create action reader for effector " << id << '\n';
                return 1;
            }
        }
    }
    std::cout << "Command & Control: DDS ready on domain 0." << std::endl;
#endif

    auto last          = std::chrono::steady_clock::now();
    auto last_republish = std::chrono::steady_clock::now();

    // Threat spawn dialog state
    int  spawn_type         = 2;     // 0=BALST  1=DRONE  2=ASCM
    bool auto_spawn_enabled = false;
    int  auto_interval_ms   = 2000;  // milliseconds between auto-spawns
    std::chrono::steady_clock::time_point last_auto_spawn = std::chrono::steady_clock::now();
    srand(static_cast<unsigned int>(SDL_GetTicks()));
    bool show_legend     = false;
    int  threats_breached = 0;
    auto last_breach_time = std::chrono::steady_clock::now() - std::chrono::seconds(60);

    while (running) {
#ifdef USE_CONNEXT
        process_qos_dialog_events();
#endif
        // Lambda: spawn a threat of the given type at a random top-of-screen position
        auto spawnThreatAtTop = [&](int type) {
            ship::Threat t{};
            t.id      = next_id++;
            t.x       = 20.0 + rand() % 760;
            t.y       = 5.0;
            t.heading = 180.0;
            switch (type) {
                case 0: t.speed = 45.0; break;  // BALST: fast ballistic
                case 1: t.speed = 15.0; break;  // DRONE: slow UAS
                case 2: t.speed = 30.0; break;  // ASCM:  cruise missile
                default: t.speed = 22.0;
            }
            t.severity = type;
            threats.push_back(t);
            publishThreat(t, threat_history[t.id]);
            last_auto_spawn = std::chrono::steady_clock::now();
        };

        // Dialog button rects — panel-positioned, DY uses panel_dialog_y from previous frame
        const int DX = 810, DY = 474, DW = 246, DH = 124;
        SDL_Rect dlg_type_btns[3] = {
            {DX+4,   DY+23, 77, 20}, {DX+84,  DY+23, 77, 20}, {DX+164, DY+23, 77, 20}
        };
        SDL_Rect dlg_btn_spawn = {DX+4, DY+47, 238, 20};
        SDL_Rect dlg_btn_auto  = {DX+4, DY+74, 116, 20};
        SDL_Rect dlg_iv_btns[4] = {
            {DX+4,   DY+98, 57, 20}, {DX+64,  DY+98, 57, 20},
            {DX+124, DY+98, 57, 20}, {DX+184, DY+98, 57, 20}
        };
        const SDL_Rect legend_btn       = {5, 575, 66, 20};
        const SDL_Rect legend_close_btn = {738, 102, 20, 18};
        auto in_rect = [](const SDL_Rect& r, int x, int y) {
            return x >= r.x && x < r.x+r.w && y >= r.y && y < r.y+r.h;
        };

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mx = e.button.x, my = e.button.y;
                if (in_rect(legend_btn, mx, my)) {
                    show_legend = !show_legend;
                } else if (show_legend) {
                    if (in_rect(legend_close_btn, mx, my)) show_legend = false;
                } else {
                    const EffectorMount* selected = nullptr;
                    int nearest_distance = 0;
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        for (const auto& mount : effector_mounts) {
                            if (!in_rect(effector_bounds(mount), mx, my)) continue;
                            const int dx = mx - mount.position.x;
                            const int dy = my - (mount.position.y - 12);
                            const int distance = dx * dx + dy * dy;
                            if (!selected || distance < nearest_distance) {
                                selected = &mount;
                                nearest_distance = distance;
                            }
                        }
                    }
                    if (selected) {
#ifdef USE_CONNEXT
                        const int id = selected->id;
                        ReaderQosTarget reader_target;
                        reader_target.name = "Action Reader";
                        reader_target.topic_name = effector_topic->get_name();
                        reader_target.scope = "Command & Control reader for this effector only.";
                        reader_target.get = [&action_readers, id](effector_qos::ReaderSettings& settings,
                                                                  std::string& error) {
                            if (!action_readers[id - 1]) { error = "Action reader is unavailable."; return false; }
                            settings = action_readers[id - 1]->settings;
                            return true;
                        };
                        reader_target.set = [&action_readers, id](const effector_qos::ReaderSettings& settings,
                                                                  std::string& error) {
                            return effector_qos::replace_reader(action_readers[id - 1], settings, error);
                        };
                        show_effector_qos_dialog(win, participant, effector_topic, qos_channel,
                            id, effector_name(id), {reader_target});
#else
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Effector QoS",
                            "Effector QoS requires a USE_CONNEXT build.", win);
#endif
                        last = last_republish = last_auto_spawn = std::chrono::steady_clock::now();
                        continue;
                    }
                    bool in_dlg = (mx >= DX && mx < DX+DW && my >= DY && my < DY+DH);
                    if (in_dlg) {
                        // Type selection
                        for (int i = 0; i < 3; i++)
                            if (in_rect(dlg_type_btns[i], mx, my)) spawn_type = i;
                        // Spawn one
                        if (in_rect(dlg_btn_spawn, mx, my)) {
#ifdef USE_CONNEXT
                            auto endpoint = show_threat_qos_dialog(win, participant, threat_topic, threat_reader_targets);
                            if (endpoint) {
                                g_configured_publishers.emplace(next_id, std::move(endpoint));
                                spawnThreatAtTop(spawn_type);
                            }
#else
                            spawnThreatAtTop(spawn_type);
#endif
                            last = last_republish = last_auto_spawn = std::chrono::steady_clock::now();
                        }
                        // Auto-spawn toggle
                        if (in_rect(dlg_btn_auto, mx, my)) auto_spawn_enabled = !auto_spawn_enabled;
                        // Interval selection
                        const int iv_vals[4] = {1000, 2000, 3000, 4000};
                        for (int i = 0; i < 4; i++)
                            if (in_rect(dlg_iv_btns[i], mx, my)) auto_interval_ms = iv_vals[i];
                    } else if (mx < 800 && my < 460) {
                        // Click-to-place threat anywhere in the game area
                        ship::Threat t{};
                        t.id      = next_id++;
                        t.x       = mx;
                        t.y       = my;
                        t.heading = 180.0;
                        switch (spawn_type) {
                            case 0: t.speed = 45.0; break;
                            case 1: t.speed = 15.0; break;
                            case 2: t.speed = 30.0; break;
                            default: t.speed = 22.0;
                        }
                        t.severity = spawn_type;
                        threats.push_back(t);
                        publishThreat(t, threat_history[t.id]);
                    }
                }
            }
        }

        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> dt = now - last;
        last = now;

        // Update threats (move toward ship bow)
        const int cx = SHIP_X, cy = SHIP_Y;
        for (auto &t : threats) {
            double dx = cx - t.x;
            double dy = (cy - 16) - t.y;   // aim at main deck (side view)
            double dist = std::sqrt(dx*dx + dy*dy);
            if (dist > 1e-6) {
                t.x += (dx / dist) * t.speed * dt.count();
                t.y += (dy / dist) * t.speed * dt.count();
            }
        }

        // Republish current threat positions every 500 ms so sensor range-checks work
        {
            auto rep_now = std::chrono::steady_clock::now();
            if (rep_now - last_republish >= std::chrono::milliseconds(500)) {
                last_republish = rep_now;
                for (auto& t : threats) publishThreat(t, threat_history[t.id]);
            }
        }

        // Auto-spawn threats at configured interval
        {
            auto now_as = std::chrono::steady_clock::now();
            if (auto_spawn_enabled &&
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now_as - last_auto_spawn).count() >= auto_interval_ms) {
                spawnThreatAtTop(rand() % 3);  // random type each interval
            }
        }

        // Cull threats that reach the hull (70 px from ship center); track hull breaches
        {
            auto before_sz = threats.size();
            threats.erase(std::remove_if(threats.begin(), threats.end(), [&](const ship::Threat &t){
                double dx = cx - t.x; double dy = cy - t.y; return (dx*dx+dy*dy) < 4900.0; }), threats.end());
            int hit = int(before_sz) - int(threats.size());
            if (hit > 0) {
                threats_breached += hit;
                last_breach_time = std::chrono::steady_clock::now();
            }
        }

        // Drain new effect events from effector listener
#ifdef USE_CONNEXT
        {
            std::lock_guard<std::mutex> lk(effector_listener.mtx);
            for (auto& ev : effector_listener.pending_fx) {
                plumes.push_back({ev.lx, ev.ly, 0.0f, 1.4f});
                // Speed varies by weapon type
                float spd = 150.0f;
                switch (ev.effector_id) {
                    case 1: spd = 150.0f; break;  // SM-2 MR
                    case 2: spd = 175.0f; break;  // SM-6
                    case 3: spd = 130.0f; break;  // ESSM
                    case 4: spd = 210.0f; break;  // CIWS
                    case 5: spd = 100.0f; break;  // MK45
                }
                interceptors.push_back({ev.lx, ev.ly, ev.target_id, spd, 0.0f, 20.0f, ev.will_kill, false});
            }
            effector_listener.pending_fx.clear();
        }
#endif
        // Age and expire effects
        float dtf = float(dt.count());
        for (auto& p : plumes) p.age += dtf;
        for (auto& b : blasts) b.age += dtf;
        plumes.erase(std::remove_if(plumes.begin(), plumes.end(),
            [](const LaunchPlume& p){ return p.age >= p.life; }), plumes.end());
        blasts.erase(std::remove_if(blasts.begin(), blasts.end(),
            [](const KillBlast& b){ return b.age >= b.life; }), blasts.end());

        // Update interceptors: track moving targets, trigger blast on contact
        std::vector<int> kill_ids;
        for (auto& ic : interceptors) {
            if (ic.done) continue;
            ic.age += dtf;
            const ship::Threat* tgt = nullptr;
            for (const auto& th : threats) { if (th.id == ic.target_id) { tgt = &th; break; } }
            if (!tgt) { ic.done = true; continue; }   // target already gone
            float dx = float(tgt->x) - ic.x, dy = float(tgt->y) - ic.y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < 10.0f) {
                ic.done = true;
                float blife = ic.will_kill ? 1.4f : 0.7f;   // bigger blast for kills
                blasts.push_back({ic.x, ic.y, 0.0f, blife});
                if (ic.will_kill) {
                    bool dup = false;
                    for (int k : kill_ids) if (k == ic.target_id) { dup = true; break; }
                    if (!dup) kill_ids.push_back(ic.target_id);
                }
            } else {
                ic.x += (dx / dist) * ic.speed * dtf;
                ic.y += (dy / dist) * ic.speed * dtf;
            }
        }
        interceptors.erase(std::remove_if(interceptors.begin(), interceptors.end(),
            [](const Interceptor& ic){ return ic.done || ic.age >= ic.life; }), interceptors.end());
        for (int kid : kill_ids)
            threats.erase(std::remove_if(threats.begin(), threats.end(),
                [kid](const ship::Threat& th){ return th.id == kid; }), threats.end());

        for (auto it = threat_history.begin(); it != threat_history.end();) {
            const int id = it->first;
            if (std::none_of(threats.begin(), threats.end(),
                    [id](const ship::Threat& t){ return t.id == id; })) {
#ifdef USE_CONNEXT
                g_configured_publishers.erase(id);
#endif
                it = threat_history.erase(it);
            } else
                ++it;
        }

        // Render
        SDL_SetRenderDrawColor(ren, 10, 20, 40, 255);   // ocean at night
        SDL_RenderClear(ren);

        // Subtle ocean surface texture
        SDL_SetRenderDrawColor(ren, 15, 30, 55, 255);
        for (int wy = 60; wy < 600; wy += 18)
            SDL_RenderDrawLine(ren, 0, wy, 799, wy);

        // Deep water below the waterline
        SDL_Rect deepsea = {0, SHIP_Y - 2, 800, 600 - (SHIP_Y - 2)};
        SDL_SetRenderDrawColor(ren, 8, 16, 32, 255);
        SDL_RenderFillRect(ren, &deepsea);

        // Radar range rings centred on ship (green, every 100 px)
        SDL_SetRenderDrawColor(ren, 20, 55, 30, 255);
        for (int ring = 100; ring <= 450; ring += 100) {
            for (int deg = 0; deg < 360; deg += 3) {
                float ang = deg * 3.14159f / 180.0f;
                SDL_RenderDrawPoint(ren, cx + int(ring * cosf(ang)), cy + int(ring * sinf(ang)));
            }
        }

        // SPY-1D detection range ring — dashed light red arc (412 px = 220 nm)
        SDL_SetRenderDrawColor(ren, 180, 45, 45, 255);
        for (int deg = 0; deg < 360; deg++) {
            if (deg % 6 >= 3) continue;   // 3-on / 3-off dash pattern
            float ang = deg * 3.14159f / 180.0f;
            int rx = cx + int(412 * cosf(ang));
            int ry = cy + int(412 * sinf(ang));
            if (rx >= 0 && rx < 800 && ry >= 0 && ry < 600)
                SDL_RenderDrawPoint(ren, rx, ry);
        }

        // Launch plumes (behind ship so hull draws on top)
        for (auto& p : plumes) draw_plume(ren, p);

        // Ship photo (or drawn fallback)
        if (ship_tex) {
            SDL_RenderCopy(ren, ship_tex, nullptr, &ship_bounds);
        } else {
            SDL_RenderSetScale(ren, 1.4f, 1.4f);
            draw_destroyer(ren, int(cx / 1.4f), int(cy / 1.4f));
            SDL_RenderSetScale(ren, 1.0f, 1.0f);
        }
        draw_effector_mounts(ren, phalanx_tex);

        // Published samples form a Shapes Demo-style trail behind the live position.
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        for (const auto& entry : threat_history) {
            Uint8 alpha = 45;
            for (const auto& sample : entry.second) {
                const float angle = atan2f(float(cy - 16) - float(sample.y),
                                          float(cx) - float(sample.x));
                draw_threat(ren, int(sample.x), int(sample.y), angle, sample.severity, alpha);
                alpha += 25;
            }
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

        // Threats (typed shapes oriented toward the ship)
        for (auto& t : threats) {
            float tdx = float(cx)      - float(t.x);
            float tdy = float(cy - 16) - float(t.y);
            draw_threat(ren, int(t.x), int(t.y), atan2f(tdy, tdx), t.severity);
        }

        // Interceptor missiles (above threats, below blasts)
        for (const auto& ic : interceptors) {
            if (ic.done) continue;
            float angle = -1.5708f;   // default: pointing up
            for (const auto& th : threats) {
                if (th.id == ic.target_id) {
                    angle = atan2f(float(th.y) - ic.y, float(th.x) - ic.x);
                    break;
                }
            }
            draw_interceptor(ren, ic.x, ic.y, angle);
        }

        // Kill/intercept blasts (topmost layer)
        for (auto& b : blasts) draw_blast(ren, b);

        // ---- Hull breach flash + warning text ----
        {
            auto breach_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - last_breach_time).count();
            // Red screen flash (fades over 1.5 s)
            if (breach_ms < 1500) {
                float frac = 1.0f - float(breach_ms) / 1500.0f;
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, 220, 15, 15, Uint8(170 * frac));
                SDL_Rect flash_r = {0, 0, 800, 600};
                SDL_RenderFillRect(ren, &flash_r);
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
            }
            // Warning text for 3 s; blinks after 1.5 s
            if (threats_breached > 0 && breach_ms < 3000) {
                bool blink_off = (breach_ms >= 1500) && ((breach_ms / 300) % 2 != 0);
                if (!blink_off) {
                    char hb[40];
                    int n = snprintf(hb, sizeof(hb), "!! HULL BREACH x%d !!", threats_breached);
                    SDL_SetRenderDrawColor(ren, 255, 50, 50, 255);
                    draw_text(ren, (800 - n * 12) / 2, 262, hb, 2);
                }
            }
        }

#ifdef USE_CONNEXT
        // Dotted radar-track lines from each detection point to ship bow
        {
            std::lock_guard<std::mutex> lk(sensor_listener.mtx);
            SDL_SetRenderDrawColor(ren, 150, 145, 25, 255);
            for (auto &[sid, d] : sensor_listener.latest) {
                int x1 = int(d.x), y1 = int(d.y), x2 = cx, y2 = cy - 16;
                float len = sqrtf(float((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)));
                int steps = int(len);
                for (int s = 0; s < steps; s += 8) {
                    float fr = float(s) / float(steps > 0 ? steps : 1);
                    SDL_RenderDrawPoint(ren, x1 + int((x2-x1)*fr), y1 + int((y2-y1)*fr));
                }
            }
        }
        // (effector visuals are handled by the plume / blast effect system above)
#endif

        // ---- Status Panel (right 250 px) ----
        {
            const int PX         = 810;   // panel left edge
            const int PW         = 246;   // panel width
            const int COL_NAME   = 813;   // system name  (9 chars = 54 px)
            const int COL_THREAT = 877;   // threat / target column
            const int COL_CONF   = 925;   // conf / status column
            const int COL_TARGET = COL_THREAT;
            const int COL_STATUS = COL_CONF;

            // Panel background + left border
            SDL_Rect pbg = {PX, 0, PW, 600};
            SDL_SetRenderDrawColor(ren, 22, 22, 32, 255);
            SDL_RenderFillRect(ren, &pbg);
            SDL_SetRenderDrawColor(ren, 65, 65, 85, 255);
            SDL_RenderDrawLine(ren, PX, 0, PX, 599);

            int pcy = 8;
            auto panel_sep = [&]() {
                SDL_SetRenderDrawColor(ren, 65, 65, 85, 255);
                SDL_RenderDrawLine(ren, PX + 2, pcy, PX + PW - 4, pcy);
                pcy += 5;
            };

            // ---- HULL STATUS ----
            {
                auto bms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - last_breach_time).count();
                bool flash2 = (bms < 2000) && ((bms / 250) % 2 == 0);
                if (threats_breached == 0) {
                    SDL_SetRenderDrawColor(ren, 55, 175, 55, 255);
                    draw_text(ren, COL_NAME, pcy, "HULL SECURE", 2);
                } else {
                    SDL_SetRenderDrawColor(ren,
                        flash2 ? 255 : 210, flash2 ? 30 : 50, flash2 ? 30 : 50, 255);
                    char hb[24]; snprintf(hb, sizeof(hb), "BREACHED: %d", threats_breached);
                    draw_text(ren, COL_NAME, pcy, hb, 2);
                }
                pcy += 20;
                panel_sep();
            }

            // ---- SENSORS ----
            SDL_SetRenderDrawColor(ren, 240, 210, 60, 255);
            draw_text(ren, COL_NAME, pcy, "SENSORS", 2);
            pcy += 20;
            panel_sep();
            SDL_SetRenderDrawColor(ren, 100, 100, 120, 255);
            draw_text(ren, COL_NAME,   pcy, "SENSOR", 1);
            draw_text(ren, COL_THREAT, pcy, "THREAT", 1);
            draw_text(ren, COL_CONF,   pcy, "CONF",   1);
            pcy += 11;
            panel_sep();
#ifdef USE_CONNEXT
            {
                std::lock_guard<std::mutex> lk2(sensor_listener.mtx);
                auto now2 = std::chrono::steady_clock::now();
                for (auto &[sid, d] : sensor_listener.latest) {
                    bool active = sensor_listener.last_seen.count(sid) > 0 &&
                                  (now2 - sensor_listener.last_seen.at(sid)) < std::chrono::seconds(5);
                    char buf[24];
                    if (active) SDL_SetRenderDrawColor(ren,  80, 220,  80, 255);
                    else        SDL_SetRenderDrawColor(ren, 110, 110, 110, 255);
                    draw_text(ren, COL_NAME, pcy, sensor_name(sid), 1);
                    if (active) {
                        snprintf(buf, sizeof(buf), "T#%-5d", d.threat_id);
                        draw_text(ren, COL_THREAT, pcy, buf, 1);
                        snprintf(buf, sizeof(buf), "%3d%%", d.confidence);
                        draw_text(ren, COL_CONF, pcy, buf, 1);
                    } else {
                        draw_text(ren, COL_THREAT, pcy, "IDLE   ", 1);
                        draw_text(ren, COL_CONF,   pcy, " --",     1);
                    }
                    pcy += 11;
                }
                if (sensor_listener.latest.empty()) {
                    SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
                    draw_text(ren, COL_NAME, pcy, "none", 1);
                    pcy += 11;
                }
            }
#else
            SDL_SetRenderDrawColor(ren, 110, 110, 110, 255);
            for (int id = 1; id <= 4; ++id) {
                draw_text(ren, COL_NAME, pcy, sensor_name(id), 1);
                draw_text(ren, COL_THREAT, pcy, " --", 1);
                draw_text(ren, COL_CONF, pcy, "NO DDS", 1);
                pcy += 11;
            }
#endif

            // ---- EFFECTORS ----
            pcy += 4;
            panel_sep();
            SDL_SetRenderDrawColor(ren, 60, 200, 240, 255);
            draw_text(ren, COL_NAME, pcy, "EFFECTORS", 2);
            pcy += 20;
            panel_sep();
            SDL_SetRenderDrawColor(ren, 100, 100, 120, 255);
            draw_text(ren, COL_NAME,   pcy, "WEAPON", 1);
            draw_text(ren, COL_TARGET, pcy, "TARGET", 1);
            draw_text(ren, COL_STATUS, pcy, "STATUS", 1);
            pcy += 11;
            panel_sep();
#ifdef USE_CONNEXT
            {
                std::lock_guard<std::mutex> lk2(effector_listener.mtx);
                auto now2 = std::chrono::steady_clock::now();
                for (auto &[eid, a] : effector_listener.latest) {
                    bool active = effector_listener.last_seen.count(eid) > 0 &&
                                  (now2 - effector_listener.last_seen.at(eid)) < std::chrono::seconds(5);
                    char buf[24];
                    if      (!active)     SDL_SetRenderDrawColor(ren, 110, 110, 110, 255);
                    else if (a.destroyed) SDL_SetRenderDrawColor(ren, 255, 160,   0, 255);
                    else                  SDL_SetRenderDrawColor(ren,  80, 220,  80, 255);
                    draw_text(ren, COL_NAME, pcy, effector_name(eid), 1);
                    if (active) {
                        snprintf(buf, sizeof(buf), "T#%-5d", a.threat_id);
                        draw_text(ren, COL_TARGET, pcy, buf, 1);
                        draw_text(ren, COL_STATUS, pcy, a.destroyed ? "KILL  " : "FIRING", 1);
                    } else {
                        draw_text(ren, COL_TARGET, pcy, " --   ", 1);
                        draw_text(ren, COL_STATUS, pcy, "IDLE  ", 1);
                    }
                    pcy += 11;
                }
                if (effector_listener.latest.empty()) {
                    SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
                    draw_text(ren, COL_NAME, pcy, "none", 1);
                }
            }
#else
            SDL_SetRenderDrawColor(ren, 110, 110, 110, 255);
            for (const auto& mount : effector_mounts) {
                draw_text(ren, COL_NAME, pcy, effector_name(mount.id), 1);
                draw_text(ren, COL_TARGET, pcy, " --", 1);
                draw_text(ren, COL_STATUS, pcy, "NO DDS", 1);
                pcy += 11;
            }
#endif

            // ---- INCOMING THREATS ----
            pcy += 4;
            panel_sep();
            SDL_SetRenderDrawColor(ren, 230, 80, 80, 255);
            draw_text(ren, COL_NAME, pcy, "THREATS", 2);
            pcy += 20;
            panel_sep();
            SDL_SetRenderDrawColor(ren, 100, 100, 120, 255);
            draw_text(ren, 813, pcy, "ID",   1);
            draw_text(ren, 845, pcy, "TYPE", 1);
            draw_text(ren, 893, pcy, "SPD",  1);
            draw_text(ren, 933, pcy, "TTI",  1);
            pcy += 11;
            panel_sep();
            if (threats.empty()) {
                SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
                draw_text(ren, 813, pcy, "NO CONTACTS", 1);
                pcy += 11;
            } else {
                for (auto& t : threats) {
                    if (pcy + 11 > DY) break;   // keep rows above the pinned controls
                    float dx_t = float(t.x) - 400.0f;
                    float dy_t = float(t.y) - 570.0f;
                    float dist_t = sqrtf(dx_t*dx_t + dy_t*dy_t);
                    float tti   = (t.speed > 0.01) ? dist_t / float(t.speed) : 99.0f;
                    int   spd_kt = int(t.speed * 30);  // scale to notional knots for display
                    const char* type_str =
                        (t.severity == 0) ? "BALST" :
                        (t.severity == 1) ? "DRONE" : "ASCM ";
                    char buf[32];
                    SDL_SetRenderDrawColor(ren, 220, 90, 90, 255);
                    snprintf(buf, sizeof(buf), "T#%d", t.id);
                    draw_text(ren, 813, pcy, buf, 1);
                    draw_text(ren, 845, pcy, type_str, 1);
                    snprintf(buf, sizeof(buf), "%dkt", spd_kt);
                    draw_text(ren, 893, pcy, buf, 1);
                    snprintf(buf, sizeof(buf), "%3.0fs", tti);
                    draw_text(ren, 933, pcy, buf, 1);
                    pcy += 11;
                }
            }

            // ---- SPAWN CONTROLS (pinned to panel bottom) ----
            {
                const int sy = 474;
                const int sx = PX;
                const int sw = PW;

                // Background
                SDL_Rect dlg = {sx, sy, sw, 124};
                SDL_SetRenderDrawColor(ren, 18, 22, 38, 255);
                SDL_RenderFillRect(ren, &dlg);

                // Title bar
                SDL_Rect title_bar = {sx, sy, sw, 20};
                SDL_SetRenderDrawColor(ren, 35, 45, 70, 255);
                SDL_RenderFillRect(ren, &title_bar);
                SDL_SetRenderDrawColor(ren, 200, 195, 80, 255);
                draw_text(ren, sx + 51, sy + 3, "SPAWN THREAT", 2);

                // Border + separator under title
                SDL_SetRenderDrawColor(ren, 70, 85, 110, 255);
                SDL_RenderDrawRect(ren, &dlg);
                SDL_RenderDrawLine(ren, sx+2, sy+20, sx+sw-2, sy+20);

                // Type buttons: [BALST] [DRONE] [ASCM ]
                const char* type_labels[] = {"BALST", "DRONE", "ASCM "};
                SDL_Rect type_r[3] = {
                    {sx+4,   sy+23, 77, 20},
                    {sx+84,  sy+23, 77, 20},
                    {sx+164, sy+23, 77, 20}
                };
                for (int i = 0; i < 3; i++) {
                    if (i == spawn_type) {
                        SDL_SetRenderDrawColor(ren, 40, 75, 45, 255);
                        SDL_RenderFillRect(ren, &type_r[i]);
                        SDL_SetRenderDrawColor(ren, 80, 210, 90, 255);
                    } else {
                        SDL_SetRenderDrawColor(ren, 28, 32, 50, 255);
                        SDL_RenderFillRect(ren, &type_r[i]);
                        SDL_SetRenderDrawColor(ren, 100, 105, 130, 255);
                    }
                    SDL_RenderDrawRect(ren, &type_r[i]);
                    draw_text(ren, type_r[i].x + 8, type_r[i].y + 3, type_labels[i], 2);
                }

                // SPAWN ONE button
                SDL_Rect spawn_r = {sx+4, sy+47, 238, 20};
                SDL_SetRenderDrawColor(ren, 30, 55, 90, 255);
                SDL_RenderFillRect(ren, &spawn_r);
                SDL_SetRenderDrawColor(ren, 60, 120, 200, 255);
                SDL_RenderDrawRect(ren, &spawn_r);
                draw_text(ren, sx + 69, sy + 50, "SPAWN ONE", 2);

                // Separator before auto section
                SDL_SetRenderDrawColor(ren, 70, 85, 110, 255);
                SDL_RenderDrawLine(ren, sx+2, sy+71, sx+sw-2, sy+71);

                // AUTO toggle button
                SDL_Rect auto_r = {sx+4, sy+74, 116, 20};
                if (auto_spawn_enabled) {
                    SDL_SetRenderDrawColor(ren, 40, 75, 45, 255);
                    SDL_RenderFillRect(ren, &auto_r);
                    SDL_SetRenderDrawColor(ren, 80, 210, 90, 255);
                } else {
                    SDL_SetRenderDrawColor(ren, 28, 32, 50, 255);
                    SDL_RenderFillRect(ren, &auto_r);
                    SDL_SetRenderDrawColor(ren, 100, 105, 130, 255);
                }
                SDL_RenderDrawRect(ren, &auto_r);
                draw_text(ren, sx + 8, sy + 77, auto_spawn_enabled ? "AUTO: ON " : "AUTO: OFF", 2);

                // INTERVAL label (right of AUTO button)
                SDL_SetRenderDrawColor(ren, 140, 145, 165, 255);
                draw_text(ren, sx + 126, sy + 77, "IVRL:", 2);

                // Interval buttons: [1s] [2s] [3s] [4s]
                const int iv_vals2[4] = {1000, 2000, 3000, 4000};
                const char* iv_labels[] = {"1s", "2s", "3s", "4s"};
                SDL_Rect iv_r[4] = {
                    {sx+4,   sy+98, 57, 20},
                    {sx+64,  sy+98, 57, 20},
                    {sx+124, sy+98, 57, 20},
                    {sx+184, sy+98, 57, 20}
                };
                for (int i = 0; i < 4; i++) {
                    if (auto_interval_ms == iv_vals2[i]) {
                        SDL_SetRenderDrawColor(ren, 65, 40, 90, 255);
                        SDL_RenderFillRect(ren, &iv_r[i]);
                        SDL_SetRenderDrawColor(ren, 170, 110, 220, 255);
                    } else {
                        SDL_SetRenderDrawColor(ren, 28, 32, 50, 255);
                        SDL_RenderFillRect(ren, &iv_r[i]);
                        SDL_SetRenderDrawColor(ren, 100, 105, 130, 255);
                    }
                    SDL_RenderDrawRect(ren, &iv_r[i]);
                    draw_text(ren, iv_r[i].x + 16, iv_r[i].y + 3, iv_labels[i], 2);
                }
            }
        }
        // ---- End Status Panel ----

        // ---- Legend button (bottom-left, always visible) ----
        {
            SDL_SetRenderDrawColor(ren, show_legend ? 45 : 28, show_legend ? 55 : 34, show_legend ? 80 : 55, 255);
            SDL_RenderFillRect(ren, &legend_btn);
            SDL_SetRenderDrawColor(ren, show_legend ? 140 : 95, show_legend ? 160 : 115, show_legend ? 220 : 175, 255);
            SDL_RenderDrawRect(ren, &legend_btn);
            SDL_SetRenderDrawColor(ren, 220, 220, 255, 255);
            draw_text(ren, legend_btn.x + 3, legend_btn.y + 4, "LEGEND", 2);
        }

        // ---- Legend overlay ----
        if (show_legend) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 0, 0, 10, 195);
            SDL_Rect dim = {0, 0, 800, 600};
            SDL_RenderFillRect(ren, &dim);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

            const int LX = 40, LY = 100, LW = 720, LH = 280;
            SDL_Rect lbox = {LX, LY, LW, LH};
            SDL_SetRenderDrawColor(ren, 14, 18, 32, 255);
            SDL_RenderFillRect(ren, &lbox);
            SDL_SetRenderDrawColor(ren, 80, 95, 140, 255);
            SDL_RenderDrawRect(ren, &lbox);

            // Title bar
            SDL_Rect lbar = {LX, LY, LW, 22};
            SDL_SetRenderDrawColor(ren, 30, 40, 70, 255);
            SDL_RenderFillRect(ren, &lbar);
            SDL_SetRenderDrawColor(ren, 80, 95, 140, 255);
            SDL_RenderDrawLine(ren, LX, LY + 22, LX + LW, LY + 22);
            SDL_SetRenderDrawColor(ren, 220, 225, 255, 255);
            draw_text(ren, LX + (LW - 13 * 12) / 2, LY + 4, "SYSTEM LEGEND", 2);

            // Close [X] button
            SDL_SetRenderDrawColor(ren, 70, 25, 25, 255);
            SDL_RenderFillRect(ren, &legend_close_btn);
            SDL_SetRenderDrawColor(ren, 200, 70, 70, 255);
            SDL_RenderDrawRect(ren, &legend_close_btn);
            draw_text(ren, legend_close_btn.x + 3, legend_close_btn.y + 3, "X", 2);

            int lcy = LY + 30;
            const int lcx = LX + 12;
            struct LegEntry { const char* abbrev; const char* desc; };

            // SENSORS
            SDL_SetRenderDrawColor(ren, 240, 210, 60, 255);
            draw_text(ren, lcx, lcy, "SENSORS", 2); lcy += 16;
            SDL_SetRenderDrawColor(ren, 80, 95, 140, 255);
            SDL_RenderDrawLine(ren, LX + 4, lcy, LX + LW - 4, lcy); lcy += 5;
            static const LegEntry s_sensors[] = {
                {"AN/SPY-1D", "Aegis Phased-Array Radar - primary long-range air defense (220nm)"},
                {"AN/SPQ-9B", "Horizon Search / Gun Fire Control Radar (40nm)"},
                {"AN/SPS-67", "Surface Search Radar (25nm)"},
                {"AN/SLQ-32", "Electronic Warfare / Electronic Support Measures (100nm)"},
            };
            SDL_SetRenderDrawColor(ren, 80, 220, 80, 255);
            for (const auto& en : s_sensors) {
                char buf[128]; snprintf(buf, sizeof(buf), "%-10s  %s", en.abbrev, en.desc);
                draw_text(ren, lcx, lcy, buf, 1); lcy += 11;
            }

            // EFFECTORS
            lcy += 10;
            SDL_SetRenderDrawColor(ren, 60, 200, 240, 255);
            draw_text(ren, lcx, lcy, "EFFECTORS", 2); lcy += 16;
            SDL_SetRenderDrawColor(ren, 80, 95, 140, 255);
            SDL_RenderDrawLine(ren, LX + 4, lcy, LX + LW - 4, lcy); lcy += 5;
            struct EffectorLegEntry { int id; const char* desc; };
            static const EffectorLegEntry s_effectors[] = {
                {1, "Standard Missile 2 Medium Range - VLS launched (75nm)"},
                {2, "Standard Missile 6 Extended Range - VLS launched (130nm)"},
                {3, "Evolved Sea Sparrow Missile - short-range VLS (27nm)"},
                {4, "Phalanx Close-In Weapon System - 20mm Gatling gun (1nm)"},
                {5, "5-inch/62 Caliber Naval Gun - surface and low-altitude threats (13nm)"},
            };
            for (const auto& en : s_effectors) {
                for (const auto& mount : effector_mounts) {
                    if (mount.id != en.id) continue;
                    SDL_SetRenderDrawColor(ren, mount.color.r, mount.color.g,
                                           mount.color.b, mount.color.a);
                    draw_text(ren, lcx, lcy, effector_name(en.id), 1);
                    break;
                }
                SDL_SetRenderDrawColor(ren, 80, 180, 255, 255);
                draw_text(ren, lcx + 12 * 6, lcy, en.desc, 1); lcy += 11;
            }

            // THREATS
            lcy += 10;
            SDL_SetRenderDrawColor(ren, 230, 80, 80, 255);
            draw_text(ren, lcx, lcy, "THREATS", 2); lcy += 16;
            SDL_SetRenderDrawColor(ren, 80, 95, 140, 255);
            SDL_RenderDrawLine(ren, LX + 4, lcy, LX + LW - 4, lcy); lcy += 5;
            static const LegEntry s_threats[] = {
                {"BALST", "Ballistic Missile - high-altitude fast-descending trajectory"},
                {"DRONE", "Unmanned Aerial System (UAS) - slow, low-altitude"},
                {"ASCM",  "Anti-Ship Cruise Missile - sea-skimming high-speed attack"},
            };
            SDL_SetRenderDrawColor(ren, 220, 90, 90, 255);
            for (const auto& en : s_threats) {
                char buf[128]; snprintf(buf, sizeof(buf), "%-10s  %s", en.abbrev, en.desc);
                draw_text(ren, lcx, lcy, buf, 1); lcy += 11;
            }
        }

        SDL_RenderPresent(ren);

        std::this_thread::sleep_for(10ms);
    }

#ifdef USE_CONNEXT
    g_configured_publishers.clear();
#endif
    if (ship_tex) SDL_DestroyTexture(ship_tex);
    if (phalanx_tex) SDL_DestroyTexture(phalanx_tex);
    IMG_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
