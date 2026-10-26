#pragma once
#include <cstdint>

// Readable ink on the menu's paper. EID markup colours are made for the dark
// game screen: on paper the light ones vanish and a flat darkening turns blue,
// red and purple into near-black. Each named EID colour gets a hand-picked ink
// with at least 4.5:1 contrast on paper (checked by tests.cpp); anything else
// is darkened only as far as it needs to be.
namespace isaac::menu_ui::ink {
inline constexpr std::uint32_t PaperColor = 0xf4ede1, Ink = 0x2a1f1d;

// sRGB channel to linear light, x 10000 (WCAG relative luminance).
inline constexpr std::uint16_t Linear[256] = {
    0,3,6,9,12,15,18,21,24,27,30,33,37,40,44,48,52,56,60,65,70,75,80,86,91,97,103,110,116,123,130,137,144,152,160,
    168,176,185,194,203,212,222,232,242,252,262,273,284,296,307,319,331,343,356,369,382,395,409,423,437,452,467,482,
    497,513,529,545,561,578,595,612,630,648,666,685,704,723,742,762,782,802,823,844,865,887,908,931,953,976,999,1022,
    1046,1070,1095,1119,1144,1170,1195,1221,1248,1274,1301,1329,1356,1384,1413,1441,1470,1500,1529,1559,1590,1620,
    1651,1683,1714,1746,1779,1812,1845,1878,1912,1946,1981,2016,2051,2086,2122,2159,2195,2232,2270,2307,2346,2384,
    2423,2462,2502,2542,2582,2623,2664,2705,2747,2789,2831,2874,2918,2961,3005,3050,3095,3140,3185,3231,3278,3325,
    3372,3419,3467,3515,3564,3613,3663,3712,3763,3813,3864,3916,3968,4020,4072,4125,4179,4233,4287,4342,4397,4452,
    4508,4564,4621,4678,4735,4793,4851,4910,4969,5029,5089,5149,5210,5271,5333,5395,5457,5520,5583,5647,5711,5776,
    5841,5906,5972,6038,6105,6172,6240,6308,6376,6445,6514,6584,6654,6724,6795,6867,6939,7011,7084,7157,7231,7305,
    7379,7454,7529,7605,7682,7758,7835,7913,7991,8070,8148,8228,8308,8388,8469,8550,8632,8714,8796,8879,8963,9047,
    9131,9216,9301,9387,9473,9560,9647,9734,9823,9911,10000};

inline constexpr unsigned Luminance(std::uint32_t rgb) {
    return (2126u * Linear[(rgb >> 16) & 255] + 7152u * Linear[(rgb >> 8) & 255] + 722u * Linear[rgb & 255]) / 10000u;
}
// WCAG contrast ratio x 100 (450 = 4.5:1).
inline constexpr unsigned Contrast(std::uint32_t a, std::uint32_t b) {
    const unsigned la = Luminance(a) + 500, lb = Luminance(b) + 500;
    return la > lb ? la * 100 / lb : lb * 100 / la;
}
inline constexpr unsigned MinimumContrast = 450;

struct Mapping { std::uint32_t screen, paper; };
// Screen colours from eid_ui/layout.hpp SetColor and NamePurple.
inline constexpr Mapping Named[] = {
    {0x0000ff, 0x1f3f9a},  // ColorBlue
    {0xff8a00, 0x9a4a00},  // ColorOrange
    {0xff0000, 0xa31d14},  // ColorRed
    {0xbfbfbf, 0x55504c},  // ColorSilver
    {0xffff00, 0x6e5200},  // ColorYellow
    {0xcc4ccc, 0x7d2a7d},  // ColorEIDObjName / NamePurple
    {0x808080, 0x5e5753},  // ColorGray
    {0x00ff00, 0x2b6b1f},  // ColorLime
    {0x00ffff, 0x0f6670},  // ColorCyan
    {0x8080ff, 0x4a3fa0},  // ColorEIDTransform
    {0xffcc40, 0x7d5000},  // ColorGold
    {0xcc80ff, 0x6a3a9a},  // ColorShinyPurple
    {0xe3c6c5, 0x7a4a48},  // ColorIsaac / BlinkBirthright
    {0xccff66, 0x4a6410},  // BlinkYellowGreen
};

// The ink to draw a screen colour with on paper.
inline constexpr std::uint32_t OnPaper(std::uint32_t rgb) {
    rgb &= 0xffffff;
    const unsigned r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
    if (r > 200 && g > 200 && b > 200 && r * 299 + g * 587 + b * 114 > 150000) return Ink;  // white / EID text
    for (const auto& m : Named) if (m.screen == rgb) return m.paper;
    // Unknown colour: keep its hue, darken in 5% steps until it reads on paper.
    for (unsigned percent = 100; percent > 0; percent -= 5) {
        const std::uint32_t c = ((r * percent / 100) << 16) | ((g * percent / 100) << 8) | (b * percent / 100);
        if (Contrast(c, PaperColor) >= MinimumContrast) return c;
    }
    return Ink;
}
}
