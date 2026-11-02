// CCDrawNode로 직접 그리는 벡터 아트 모음: 칭호 아이콘(Glyph), 반짝임, 피니셔(execution)
// 아이콘.
// 다각형을 삼각형으로 채우고 검은 외곽선(kInk)을 두르는 헬퍼 위에 모양을 쌓는다.
// 번들 이미지 파일이 있으면 그것을 우선 쓰고, 없을 때 벡터로 그린 모양을 대체로 쓴다.
#pragma once
#include <Geode/Geode.hpp>
#include "SpriteFrames.hpp"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <tuple>
#include <vector>

namespace versus::art {
using namespace cocos2d;
// 다각형/선의 점 목록.
using Path = std::vector<CCPoint>;

inline constexpr float kPi = 3.14159265f;
// 외곽선에 쓰는 어두운 색.
inline constexpr ccColor4F kInk{.05f, .05f, .09f, 1.f};

// 색 변환/혼합 헬퍼: RGB를 RGBA로 바꾸고, 두 색을 섞고, 밝게/어둡게 만든다.
inline ccColor4F f4(ccColor3B c, float a = 1.f) {
    return {c.r / 255.f, c.g / 255.f, c.b / 255.f, a};
}
inline ccColor3B mixc(ccColor3B a, ccColor3B b, float t) {
    return ccc3(static_cast<GLubyte>(a.r + (b.r - a.r) * t), static_cast<GLubyte>(a.g + (b.g - a.g) * t),
                static_cast<GLubyte>(a.b + (b.b - a.b) * t));
}
inline ccColor3B light(ccColor3B c, float t) {
    return mixc(c, ccc3(255, 255, 255), t);
}
inline ccColor3B dark(ccColor3B c, float t) {
    return mixc(c, ccc3(0, 0, 0), t);
}

// 점 목록의 무게중심.
inline CCPoint centroid(Path const& p) {
    CCPoint sum{0.f, 0.f};
    for (auto const& q : p) sum = sum + q;
    return p.empty() ? sum : sum * (1.f / static_cast<float>(p.size()));
}
// 점 목록 변환 헬퍼: pivot 기준 회전(도), 평행 이동, 기준점 중심 확대.
inline Path rotated(Path p, float degrees, CCPoint pivot = {0.f, 0.f}) {
    float const a = degrees * kPi / 180.f, c = std::cos(a), s = std::sin(a);
    for (auto& q : p) {
        auto const d = q - pivot;
        q = pivot + CCPoint{d.x * c - d.y * s, d.x * s + d.y * c};
    }
    return p;
}
inline Path moved(Path p, CCPoint by) {
    for (auto& q : p) q = q + by;
    return p;
}
inline Path scaled(Path p, float k, CCPoint about) {
    for (auto& q : p) q = about + (q - about) * k;
    return p;
}

// 다각형을 중심점에서 삼각형 팬으로 채운다. 불투명한 색일 때는 중심에서 꼭짓점으로 가는
// 선을 먼저 그어
// 삼각형 사이에 생기는 틈을 메운다.
inline void fill(CCDrawNode* d, Path const& p, ccColor4F color, CCPoint const* center = nullptr) {
    if (p.size() < 3) return;
    auto* dn = d;
    auto const o = center ? *center : centroid(p);
    if (color.a >= .99f)
        for (auto const& q : p) {
            auto const d = q - o;
            float const len = std::sqrt(d.x * d.x + d.y * d.y);
            if (len > 1.4f) dn->drawSegment(o, q - d * (.7f / len), .5f, color);
        }
    for (size_t i = 0; i < p.size(); ++i) {
        CCPoint tri[3]{o, p[i], p[(i + 1) % p.size()]};
        d->drawPolygon(tri, 3, color, 0.f, color);
    }
}
// 점 목록을 선분으로 잇는다(closed면 마지막 점과 첫 점도 잇는다).
inline void stroke(CCDrawNode* d, Path const& p, float radius, ccColor4F color, bool closed = true) {
    if (p.size() < 2) return;
    size_t const n = closed ? p.size() : p.size() - 1;
    for (size_t i = 0; i < n; ++i) d->drawSegment(p[i], p[(i + 1) % p.size()], radius, color);
}
// 검은 외곽선(ink), 채움, 같은 색 가는 테두리 순서로 도형을 그린다.
// pieces가 있으면 채움만 조각별로 나눠서 그린다.
inline void shape(CCDrawNode* d, Path const& outline, ccColor4F color, float ink = 1.1f,
                  std::initializer_list<Path> pieces = {}) {
    if (ink > 0.f) stroke(d, outline, ink, kInk);
    if (pieces.size() == 0)
        fill(d, outline, color);
    else
        for (auto const& piece : pieces) fill(d, piece, color);
    stroke(d, outline, .32f, color);
}
// 외곽선이 있는 원.
inline void disc(CCDrawNode* d, CCPoint at, float r, ccColor4F color, float ink = 1.1f) {
    if (ink > 0.f) d->drawDot(at, r + ink, kInk);
    d->drawDot(at, r, color);
}
// 원호/원/별/사각형 경로를 만든다. 각도는 라디안이며, starPath의 rotation만 도 단위.
inline Path arc(CCPoint c, float r, float a0, float a1, int steps) {
    Path p;
    for (int i = 0; i <= steps; ++i) {
        float const a = a0 + (a1 - a0) * i / steps;
        p.push_back(c + CCPoint{std::cos(a) * r, std::sin(a) * r});
    }
    return p;
}
inline Path circle(CCPoint c, float r, int steps = 28) {
    auto p = arc(c, r, 0.f, 2.f * kPi, steps);
    p.pop_back();
    return p;
}
inline Path starPath(float outer, float inner, int points = 5, float rotation = 90.f) {
    Path p;
    for (int i = 0; i < points * 2; ++i) {
        float const a = (rotation + i * 180.f / points) * kPi / 180.f, r = i % 2 ? inner : outer;
        p.push_back({std::cos(a) * r, std::sin(a) * r});
    }
    return p;
}
inline Path rect(float x0, float y0, float x1, float y1) {
    return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}
// 빛나는 점(투명도가 다른 세 겹의 원)과 4각 별 모양 반짝임.
inline void glowDot(CCDrawNode* d, CCPoint at, float r, ccColor3B c, float a) {
    d->drawDot(at, r, f4(c, a * .18f));
    d->drawDot(at, r * .62f, f4(c, a * .3f));
    d->drawDot(at, r * .3f, f4(c, a * .5f));
}
inline void glint(CCDrawNode* d, CCPoint at, float r, float a = 1.f) {
    fill(d, moved(starPath(r, r * .22f, 4, 90.f), at), {1.f, 1.f, 1.f, a});
}

// 반지름 R 원에서 bite 위치의 반지름 r 원을 도려낸 초승달 모양을 그린다.
inline void crescent(CCDrawNode* d, float R, CCPoint bite, float r, ccColor4F color, float ink) {
    float const dist = std::sqrt(bite.x * bite.x + bite.y * bite.y);
    float const phi = std::atan2(bite.y, bite.x);
    float const k = std::clamp((R * R + dist * dist - r * r) / (2.f * R * dist), -1.f, 1.f);
    float const alpha = std::acos(k);
    int constexpr n = 24;
    float const t0 = phi + alpha, t1 = phi + 2.f * kPi - alpha;
    CCPoint const p0{std::cos(t0) * R, std::sin(t0) * R}, p1{std::cos(t1) * R, std::sin(t1) * R};
    float a = std::atan2(p0.y - bite.y, p0.x - bite.x), b = std::atan2(p1.y - bite.y, p1.x - bite.x);
    float ccw = std::fmod(b - a + 4.f * kPi, 2.f * kPi);
    float through = std::fmod(phi + kPi - a + 4.f * kPi, 2.f * kPi);
    float const span = through < ccw ? ccw : -(2.f * kPi - ccw);
    Path outer = arc({0.f, 0.f}, R, t0, t1, n), inner = arc(bite, r, a, a + span, n);
    Path outline = outer;
    for (auto it = inner.rbegin(); it != inner.rend(); ++it) outline.push_back(*it);
    if (ink > 0.f) stroke(d, outline, ink, kInk);
    for (int i = 0; i < n; ++i) {
        CCPoint q1[3]{outer[i], outer[i + 1], inner[i + 1]}, q2[3]{outer[i], inner[i + 1], inner[i]};
        d->drawPolygon(q1, 3, color, 0.f, color);
        d->drawPolygon(q2, 3, color, 0.f, color);
    }
    stroke(d, outline, .32f, color);
}

// 칭호 아이콘 종류 (Achievements.hpp의 TitleIcon과 같은 순서).
enum class Glyph : uint8_t {
    Shield,
    Star,
    Sword,
    Crown,
    Flame,
    Bolt,
    Gem,
    Skull,
    Heart,
    Trophy,
    Moon,
    Coin,
    Clock,
    Wing,
    People,
    Home,
    Target
};

// 검 모양을 그린다. angle(도)로 회전하고 k로 크기를, at으로 위치를 조절한다.
inline void sword(CCDrawNode* d, ccColor3B tint, float angle, float k = 1.f, CCPoint at = {0.f, 0.f}) {
    auto place = [&](Path p) {
        for (auto& q : p) q = q * k;
        return moved(rotated(p, angle), at);
    };
    auto const steel = ccc3(226, 234, 248);
    shape(d, place({{-1.9f, -2.6f}, {1.9f, -2.6f}, {1.9f, 6.4f}, {0.f, 10.f}, {-1.9f, 6.4f}}), f4(steel), 1.f * k);
    fill(d, place({{0.f, -2.6f}, {1.9f, -2.6f}, {1.9f, 6.4f}, {0.f, 10.f}}), f4(ccc3(160, 172, 196)));
    d->drawSegment(place({{0.f, -2.f}})[0], place({{0.f, 8.f}})[0], .28f * k, {1.f, 1.f, 1.f, .8f});
    shape(d, place(rect(-5.6f, -4.6f, 5.6f, -2.4f)), f4(tint), 1.f * k);
    shape(d, place(rect(-1.1f, -8.6f, 1.1f, -4.6f)), f4(dark(tint, .45f)), 1.f * k);
    auto const pommel = place({{0.f, -9.4f}})[0];
    disc(d, pommel, 1.8f * k, f4(tint), 1.f * k);
}

// 칭호 아이콘 종류별 벡터 그림. 대략 20x20 크기 안에서 원점 중심으로 그린다.
inline void drawGlyph(CCDrawNode* d, Glyph kind, ccColor3B tint) {
    auto const base = f4(tint), hi = f4(light(tint, .5f)), lo = f4(dark(tint, .35f));
    ccColor4F const white{1.f, 1.f, 1.f, 1.f};
    switch (kind) {
    case Glyph::Shield: {
        Path p{{-8, 8.4f}, {-3, 9.5f}, {0, 10},  {3, 9.5f},   {8, 8.4f},   {8, 1.5f},
               {6, -3.6f}, {3, -7.6f}, {0, -10}, {-3, -7.6f}, {-6, -3.6f}, {-8, 1.5f}};
        shape(d, p, base);
        Path left{{0, 10}, {-3, 9.5f}, {-8, 8.4f}, {-8, 1.5f}, {-6, -3.6f}, {-3, -7.6f}, {0, -10}};
        CCPoint c{-3.5f, 1.f};
        fill(d, left, f4(light(tint, .3f)), &c);
        stroke(d, scaled(p, .7f, {0, .5f}), .45f, lo);
        fill(d, moved(starPath(3.6f, 1.5f), {0, 1}), white);
        break;
    }
    case Glyph::Star: {
        auto p = starPath(10.f, 4.3f);
        stroke(d, p, 1.15f, kInk);
        for (size_t i = 0; i < p.size(); ++i) {
            CCPoint tri[3]{{0, 0}, p[i], p[(i + 1) % p.size()]};
            auto const c = i % 2 ? base : hi;
            d->drawPolygon(tri, 3, c, 0.f, c);
        }
        stroke(d, p, .3f, base);
        d->drawDot({-1.2f, 2.f}, 1.3f, {1, 1, 1, .7f});
        break;
    }
    case Glyph::Sword: sword(d, tint, -45.f); break;
    case Glyph::Crown: {
        Path outline{{-8, -7}, {8, -7}, {8, -3}, {9, 6}, {4.5f, .5f}, {0, 8.5f}, {-4.5f, .5f}, {-9, 6}, {-8, -3}};
        shape(d, outline, base, 1.1f,
              {rect(-8, -7, 8, 0),
               {{-8.4f, -1}, {-9, 6}, {-3.4f, -1}},
               {{-3.8f, -1}, {0, 8.5f}, {3.8f, -1}},
               {{3.4f, -1}, {9, 6}, {8.4f, -1}}});
        fill(d, rect(-8, -7, 8, -3.2f), lo);
        stroke(d, {{-8, -3.1f}, {8, -3.1f}}, .4f, kInk, false);
        for (auto tip : {CCPoint{-9, 6}, CCPoint{0, 8.5f}, CCPoint{9, 6}}) disc(d, tip, 1.5f, white, .7f);
        disc(d, {0, -5.1f}, 1.4f, f4(ccc3(120, 230, 255)), .5f);
        disc(d, {-4.6f, -5.1f}, 1.1f, f4(ccc3(255, 110, 140)), .5f);
        disc(d, {4.6f, -5.1f}, 1.1f, f4(ccc3(255, 110, 140)), .5f);
        break;
    }
    case Glyph::Flame: {
        Path p{{0, 10},  {3, 5.6f},      {6, 2.6f},      {7, -2},  {5.6f, -6.6f}, {2.6f, -9.3f},
               {0, -10}, {-2.6f, -9.3f}, {-5.6f, -6.6f}, {-7, -2}, {-6.4f, 2.4f}, {-3.6f, 5.4f}};
        CCPoint c{0, -3};
        stroke(d, p, 1.1f, kInk);
        fill(d, p, f4(ccc3(255, 110, 30)), &c);
        stroke(d, p, .3f, f4(ccc3(255, 110, 30)));
        CCPoint c2{0, -4.5f};
        fill(d, scaled(p, .64f, {0, -6}), f4(ccc3(255, 196, 60)), &c2);
        CCPoint c3{0, -6.f};
        fill(d, scaled(p, .32f, {0, -7.5f}), f4(ccc3(255, 248, 210)), &c3);
        break;
    }
    case Glyph::Bolt: {
        Path a{{2.8f, 10}, {-6.5f, -1}, {-.8f, -1}, {.8f, 2}}, b{{.8f, 2}, {-.8f, -1}, {-3, -10}, {6.5f, 2}};
        Path outline{{2.8f, 10}, {-6.5f, -1}, {-.8f, -1}, {-3, -10}, {6.5f, 2}, {.8f, 2}};
        shape(d, outline, base, 1.1f, {a, b});
        fill(d, {{2.8f, 10}, {-6.5f, -1}, {-2.f, -1}}, hi);
        glint(d, {3.6f, 6.5f}, 2.6f, .9f);
        break;
    }
    case Glyph::Gem: {
        Path outline{{-4.5f, 8}, {4.5f, 8}, {9, 3}, {0, -10}, {-9, 3}};
        stroke(d, outline, 1.1f, kInk);
        fill(d, {{-9, 3}, {-4.5f, 8}, {-2.5f, 3}}, hi);
        fill(d, {{-4.5f, 8}, {4.5f, 8}, {2.5f, 3}, {-2.5f, 3}}, f4(light(tint, .7f)));
        fill(d, {{4.5f, 8}, {9, 3}, {2.5f, 3}}, base);
        fill(d, {{-9, 3}, {-2.5f, 3}, {0, -10}}, base);
        fill(d, {{-2.5f, 3}, {2.5f, 3}, {0, -10}}, hi);
        fill(d, {{2.5f, 3}, {9, 3}, {0, -10}}, lo);
        ccColor4F const facet{1, 1, 1, .35f};
        stroke(d, {{-9, 3}, {9, 3}}, .3f, facet, false);
        stroke(d, {{-2.5f, 3}, {0, -10}, {2.5f, 3}}, .3f, facet, false);
        stroke(d, outline, .3f, base);
        glint(d, {-4.2f, 5.8f}, 2.8f);
        break;
    }
    case Glyph::Skull: {
        d->drawDot({0, 2}, 8.6f, kInk);
        shape(d, {{-4.6f, -2}, {4.6f, -2}, {4, -8.5f}, {-4, -8.5f}}, base);
        d->drawDot({0, 2}, 7.5f, base);
        d->drawDot({-2.6f, 5.4f}, 2.2f, f4(light(tint, .5f), .8f));
        d->drawDot({-3, 1}, 2.3f, kInk);
        d->drawDot({3, 1}, 2.3f, kInk);
        fill(d, {{0, -1.2f}, {-1.2f, -3.4f}, {1.2f, -3.4f}}, kInk);
        for (float x : {-2.f, 0.f, 2.f}) d->drawSegment({x, -5.f}, {x, -8.f}, .35f, kInk);
        break;
    }
    case Glyph::Heart: {
        d->drawDot({-4, 3}, 6.2f, kInk);
        d->drawDot({4, 3}, 6.2f, kInk);
        stroke(d, {{-8.8f, 1.2f}, {8.8f, 1.2f}, {0, -9.6f}}, 1.1f, kInk);
        fill(d, {{-8.8f, 1.2f}, {8.8f, 1.2f}, {0, -9.6f}}, base);
        d->drawDot({-4, 3}, 5.1f, base);
        d->drawDot({4, 3}, 5.1f, base);
        d->drawDot({-4.6f, 4.4f}, 2.2f, f4(light(tint, .55f)));
        d->drawDot({-5.2f, 5.2f}, .9f, white);
        break;
    }
    case Glyph::Trophy: {
        for (float side : {-1.f, 1.f}) {
            auto handle =
                arc({side * 6.f, 4.6f}, 3.2f, side > 0 ? -kPi / 2 : kPi / 2, side > 0 ? kPi / 2 : 3 * kPi / 2, 10);
            stroke(d, handle, 1.9f, kInk, false);
            stroke(d, handle, .9f, base, false);
        }
        shape(d, rect(-5.6f, -10, 5.6f, -7), lo);
        shape(d, rect(-1.2f, -7, 1.2f, -1.5f), base);
        Path cup{{-6.6f, 9}, {6.6f, 9}, {6, 4}, {4.5f, .6f}, {2, -1.6f}, {-2, -1.6f}, {-4.5f, .6f}, {-6, 4}};
        shape(d, cup, base);
        fill(d, {{-6.6f, 9}, {-2.6f, 9}, {-1.6f, -1.4f}, {-2, -1.6f}, {-4.5f, .6f}, {-6, 4}}, hi);
        fill(d, moved(starPath(2.8f, 1.2f), {.6f, 4.6f}), white);
        break;
    }
    case Glyph::Moon: {
        crescent(d, 9.5f, {4.6f, 3.f}, 7.8f, base, 1.1f);
        d->drawDot({-6.2f, -1.f}, 1.3f, lo);
        d->drawDot({-3.6f, -5.6f}, .9f, lo);
        stroke(d, arc({0, 0}, 8.4f, 2.1f, 3.3f, 8), .45f, {1, 1, 1, .7f}, false);
        glint(d, {6.f, -6.f}, 2.2f, .9f);
        break;
    }
    case Glyph::Coin: {
        disc(d, {0, 0}, 9.4f, base);
        d->drawDot({0, 0}, 7.f, hi);
        d->drawDot({0, 0}, 6.f, base);
        fill(d, starPath(4.f, 1.7f), hi);
        stroke(d, arc({0, 0}, 8.2f, 1.9f, 2.9f, 8), .5f, {1, 1, 1, .85f}, false);
        break;
    }
    case Glyph::Clock: {
        disc(d, {0, 0}, 9.4f, base);
        d->drawDot({0, 0}, 7.4f, f4(light(tint, .85f)));
        for (int i = 0; i < 12; ++i) {
            float const a = i * kPi / 6.f, r0 = i % 3 ? 6.2f : 5.3f;
            d->drawSegment({std::cos(a) * r0, std::sin(a) * r0}, {std::cos(a) * 6.8f, std::sin(a) * 6.8f}, .3f, kInk);
        }
        d->drawSegment({0, 0}, {0, 4.6f}, .75f, kInk);
        d->drawSegment({0, 0}, {3.8f, -2.2f}, .6f, kInk);
        disc(d, {0, 0}, 1.1f, base, .4f);
        break;
    }
    case Glyph::Wing: {
        float const lengths[3]{18.f, 15.f, 11.5f}, angles[3]{58.f, 34.f, 10.f};
        ccColor4F const colors[3]{hi, base, lo};
        for (int i = 0; i < 3; ++i) {
            float const L = lengths[i];
            Path f{{0, -1.4f}, {L * .3f, 2.6f}, {L, .8f}, {L * .96f, -.5f}, {L * .4f, -2.3f}};
            auto p = moved(rotated(f, angles[i]), {-8.f, -7.f});
            shape(d, p, colors[i], 1.f);
        }
        glint(d, {1.5f, 6.5f}, 2.4f, .85f);
        break;
    }
    case Glyph::People: {
        auto person = [&](CCPoint at, ccColor4F c) {
            Path body = arc(at + CCPoint{0, -9.5f}, 5.6f, 0.f, kPi, 12);
            shape(d, body, c, 1.f);
            disc(d, at + CCPoint{0, -.6f}, 3.3f, c, 1.f);
        };
        person({3.6f, 3.6f}, lo);
        person({-2.6f, 2.4f}, base);
        d->drawDot({-3.6f, 2.6f}, 1.f, {1, 1, 1, .8f});
        break;
    }
    case Glyph::Home: {
        shape(d, rect(4.4f, 3, 6.6f, 9), lo);
        shape(d, rect(-7, -9.4f, 7, 1.6f), f4(light(tint, .55f)));
        shape(d, {{-10, 1}, {0, 10}, {10, 1}}, base);
        shape(d, rect(-2.2f, -9.4f, 2.2f, -2.8f), lo, .8f);
        shape(d, rect(3, -4.4f, 5.6f, -1.6f), f4(ccc3(255, 236, 150)), .7f);
        break;
    }
    case Glyph::Target: {
        disc(d, {0, 0}, 9.4f, base);
        d->drawDot({0, 0}, 7.f, white);
        d->drawDot({0, 0}, 4.7f, base);
        d->drawDot({0, 0}, 2.4f, white);
        d->drawDot({0, 0}, 1.1f, base);
        d->drawSegment({1, 1}, {9.2f, 9.2f}, 1.3f, kInk);
        d->drawSegment({1, 1}, {9.2f, 9.2f}, .55f, f4(ccc3(250, 230, 180)));
        shape(d, {{7.2f, 9.6f}, {10.4f, 10.4f}, {9.6f, 7.2f}, {8.4f, 8.4f}}, f4(ccc3(255, 120, 120)), .7f);
        break;
    }
    }
}

// 번들 이미지 파일을 높이에 맞춰 불러온다. 없으면 nullptr.
inline CCSprite* artSprite(std::string const& file, float height) {
    auto const resource = geode::Mod::get()->expandSpriteName(file.c_str());
    auto* sprite = versus::fileSprite(std::string(resource).c_str());
    if (!sprite) return nullptr;
    sprite->setScale(height / std::max(1.f, sprite->getContentSize().height));
    return sprite;
}
// 크기 size의 투명도 전파 컨테이너 노드.
inline CCNodeRGBA* artHolder(float size) {
    auto* holder = CCNodeRGBA::create();
    holder->setCascadeOpacityEnabled(true);
    holder->setContentSize({size, size});
    holder->setAnchorPoint({.5f, .5f});
    holder->ignoreAnchorPointForPosition(false);
    return holder;
}
// <name>-base.png(색을 입힘)에 <name>-line.png(외곽선)를 겹친 이미지. base가 없으면
// nullptr.
inline CCNodeRGBA* layeredArt(std::string const& name, ccColor3B tint, float height) {
    auto* base = artSprite(name + "-base.png", height);
    if (!base) return nullptr;
    auto* holder = artHolder(height);
    base->setColor(tint);
    base->setPosition({height / 2.f, height / 2.f});
    holder->addChild(base);
    if (auto* line = artSprite(name + "-line.png", height)) {
        line->setPosition({height / 2.f, height / 2.f});
        holder->addChild(line, 1);
    }
    return holder;
}
// Glyph 종류에 해당하는 이미지 이름. 범위를 벗어나면 star.
inline char const* glyphFile(Glyph kind) {
    static char const* const names[] = {"shield", "star", "sword", "crown", "flame", "bolt",   "gem",  "skull", "heart",
                                        "trophy", "moon", "coin",  "clock", "wing",  "people", "home", "target"};
    auto const index = static_cast<size_t>(kind);
    return index < std::size(names) ? names[index] : "star";
}
// 칭호 아이콘 노드. 이미지가 있으면 쓰고, 없으면 벡터 그림을 절반 크기로 쓴다.
inline CCNode* glyph(Glyph kind, ccColor3B tint) {
    if (auto* art = layeredArt(std::string("glyph-") + glyphFile(kind), tint, 12.f)) return art;
    auto* holder = CCNodeRGBA::create();
    holder->setCascadeOpacityEnabled(true);
    auto* d = CCDrawNode::create();
    drawGlyph(d, kind, tint);
    d->setScale(.5f);
    holder->addChild(d);
    return holder;
}
// 반짝임 아이콘. 이미지가 없으면 Star 아이콘으로 대체한다.
inline CCNode* sparkle(ccColor3B tint) {
    if (auto* sprite = artSprite("sparkle.png", 10.f)) {
        sprite->setColor(tint);
        auto* holder = artHolder(10.f);
        sprite->setPosition({5.f, 5.f});
        holder->addChild(sprite);
        return holder;
    }
    return glyph(Glyph::Star, tint);
}

// Phoenix 그림의 색 세트(주색, 보조색, 어두운 색).
struct PhoenixColors {
    ccColor3B primary = ccc3(255, 120, 24);
    ccColor3B secondary = ccc3(255, 218, 70);
    ccColor3B deep = ccc3(196, 52, 18);
};
inline CCPoint phoenixShoulder() {
    return {1.f, 3.f};
}

// Phoenix의 날개(깃털 4장)를 그린다. a는 투명도, ink는 외곽선 두께.
inline void phoenixWing(CCDrawNode* d, PhoenixColors const& c, float a = 1.f, float ink = 1.f) {
    Path const feathers[4]{
        {{2, 1}, {-9, 19}, {-5, 3}},
        {{0, 1}, {-15, 16}, {-8, 1}},
        {{-2, 0}, {-19, 10}, {-10, -1}},
        {{-4, -1}, {-20, 3}, {-10, -3}},
    };
    ccColor3B const colors[4]{c.secondary, c.primary, c.secondary, c.deep};
    for (int i = 3; i >= 0; --i) {
        if (ink > 0.f) stroke(d, feathers[i], ink, {kInk.r, kInk.g, kInk.b, a});
        fill(d, feathers[i], f4(colors[i], a));
        stroke(d, feathers[i], .3f, f4(colors[i], a));
    }
    fill(d, {{1, 2}, {-7, 15}, {-3.5f, 3.5f}}, f4(light(c.secondary, .55f), a * .8f));
}

// Phoenix의 꼬리, 몸통, 머리를 그린다.
inline void phoenixBody(CCDrawNode* d, PhoenixColors const& c, float a = 1.f, float ink = 1.f) {
    ccColor4F const inkA{kInk.r, kInk.g, kInk.b, a};
    auto plate = [&](Path const& p, ccColor3B col) {
        if (ink > 0.f) stroke(d, p, ink, inkA);
        fill(d, p, f4(col, a));
        stroke(d, p, .3f, f4(col, a));
    };
    plate({{-7, 1.5f}, {-27, 8}, {-10, -1}}, c.secondary);
    plate({{-7, -.5f}, {-30, -1.5f}, {-8, -3}}, c.primary);
    plate({{-7, -2}, {-25, -11}, {-6, -4.5f}}, c.deep);
    plate({{6, 7}, {11, 2}, {6, -3}, {-5, -4.5f}, {-10, 0}, {-4, 4.5f}}, c.primary);
    fill(d, {{6, -1}, {-4, -3.6f}, {-8, -.6f}, {2, .5f}}, f4(c.secondary, a));
    plate({{5, 8}, {1, 15}, {7.5f, 8.8f}}, c.deep);
    plate({{6.5f, 8.6f}, {6, 17}, {9.5f, 8.6f}}, c.secondary);
    plate({{5, 8.5f}, {11.5f, 9}, {14, 5}, {11, 1.5f}, {5, 3}}, c.primary);
    plate({{12.5f, 7}, {18.5f, 4.6f}, {12.5f, 3.4f}}, c.secondary);
    fill(d, rect(8.2f, 4.4f, 11.2f, 7.4f), {1, 1, 1, a});
    fill(d, rect(9.6f, 5.f, 10.9f, 6.6f), inkA);
}

// 한 가지 색으로 칠한 Phoenix 실루엣.
inline void phoenixSilhouette(CCDrawNode* d, ccColor3B tint, float a) {
    PhoenixColors c{tint, tint, tint};
    phoenixBody(d, c, a, 0.f);
    Path const feathers[4]{
        {{2, 1}, {-9, 19}, {-5, 3}},
        {{0, 1}, {-15, 16}, {-8, 1}},
        {{-2, 0}, {-19, 10}, {-10, -1}},
        {{-4, -1}, {-20, 3}, {-10, -3}},
    };
    for (auto const& f : feathers) fill(d, moved(f, phoenixShoulder()), f4(tint, a));
}

// 피니셔 아이콘의 둥근 배경판(accent 색 테두리와 은은한 빛).
inline void medallion(CCDrawNode* d, ccColor3B accent) {
    d->drawDot({0, 0}, 19.f, kInk);
    d->drawDot({0, 0}, 17.6f, f4(dark(accent, .78f)));
    d->drawDot({0, 3.5f}, 14.f, f4(dark(accent, .62f), .7f));
    glowDot(d, {0, 0}, 16.f, accent, .55f);
    stroke(d, circle({0, 0}, 17.6f, 40), .8f, f4(accent, .95f));
    stroke(d, arc({0, 0}, 16.f, 2.2f, 3.2f, 10), .5f, {1, 1, 1, .45f}, false);
}

// 피니셔(execution) 번호별 아이콘의 벡터 그림. kind는 kExecutionNames의 순서와 같다.
inline void finisherArt(CCDrawNode* d, int kind) {
    ccColor4F const white{1, 1, 1, 1};
    switch (kind) {
    case 0: {
        medallion(d, ccc3(150, 200, 255));
        Path hex{{0, 11}, {9.5f, 5.5f}, {9.5f, -5.5f}, {0, -11}, {-9.5f, -5.5f}, {-9.5f, 5.5f}};
        stroke(d, hex, 1.2f, kInk);
        fill(d, {{0, 11}, {9.5f, 5.5f}, {0, 0}, {-9.5f, 5.5f}}, f4(ccc3(252, 252, 255)));
        fill(d, {{-9.5f, 5.5f}, {0, 0}, {0, -11}, {-9.5f, -5.5f}}, f4(ccc3(206, 216, 240)));
        fill(d, {{0, 0}, {9.5f, 5.5f}, {9.5f, -5.5f}, {0, -11}}, f4(ccc3(150, 164, 204)));
        stroke(d, {{-9.5f, 5.5f}, {0, 0}, {9.5f, 5.5f}}, .45f, kInk, false);
        stroke(d, {{0, 0}, {0, -11}}, .45f, kInk, false);
        ccColor4F const pip{.12f, .13f, .24f, 1.f};
        d->drawDot({0, 5.5f}, 1.7f, f4(ccc3(240, 70, 90)));
        d->drawDot({-6.6f, .4f}, 1.35f, pip);
        d->drawDot({-2.9f, -6.f}, 1.35f, pip);
        d->drawDot({2.4f, -1.2f}, 1.3f, pip);
        d->drawDot({4.75f, -2.75f}, 1.3f, pip);
        d->drawDot({7.1f, -4.3f}, 1.3f, pip);
        glint(d, {-5.f, 8.f}, 2.6f, .9f);
        break;
    }
    case 1: {
        medallion(d, ccc3(120, 210, 255));
        d->drawSegment({-14, -8}, {14, -8}, .9f, f4(ccc3(155, 225, 255), .8f));
        auto spike = [&](float x, float h, float w) {
            Path p{{x - w, -7.5f}, {x + w, -7.5f}, {x, -7.5f + h}};
            stroke(d, p, 1.2f, f4(ccc3(170, 235, 255)));
            fill(d, p, f4(ccc3(14, 18, 34)));
            fill(d, {{x - w * .55f, -6.5f}, {x, -6.5f}, {x, -7.5f + h * .78f}}, f4(ccc3(48, 70, 120)));
        };
        spike(-8.5f, 11.f, 4.5f);
        spike(8.5f, 11.f, 4.5f);
        spike(0.f, 15.f, 5.2f);
        for (int i = 0; i < 3; ++i)
            d->drawSegment({-12.f + i * 3.f, 8.f - i * 2.5f}, {-6.f + i * 3.f, 8.f - i * 2.5f}, .45f, {1, 1, 1, .6f});
        break;
    }
    case 2: {
        medallion(d, ccc3(255, 150, 60));
        CCPoint const c{3.5f, 3.5f};
        Path tail{{8.2f, -1.4f}, {-1.4f, 8.2f}, {-12.f, -12.f}};
        stroke(d, tail, 1.1f, kInk);
        fill(d, tail, f4(ccc3(255, 96, 24)));
        fill(d, {{6.4f, -2.6f}, {1.2f, 1.2f}, {-6.f, -14.f}}, f4(ccc3(255, 150, 40)));
        fill(d, {{-2.6f, 6.4f}, {1.2f, 1.2f}, {-14.f, -6.f}}, f4(ccc3(255, 150, 40)));
        fill(d, scaled(tail, .55f, c), f4(ccc3(255, 214, 80)));
        glowDot(d, c, 12.f, ccc3(255, 180, 80), .6f);
        disc(d, c, 7.f, f4(ccc3(255, 110, 30)));
        d->drawDot(c, 5.2f, f4(ccc3(255, 196, 64)));
        d->drawDot(c + CCPoint{1.2f, 1.2f}, 2.8f, f4(ccc3(255, 250, 220)));
        break;
    }
    case 3: {
        medallion(d, ccc3(90, 240, 220));
        Path barrel = rect(-2, -1.6f, 13, 1.6f), body{{-12, 4.5f}, {1, 4.5f}, {3, -2}, {-12, -2}},
             grip{{-9.5f, -2}, {-4.5f, -2}, {-6.5f, -10.5f}, {-11.f, -9.5f}};
        auto const metal = f4(ccc3(70, 82, 108)), steel = f4(ccc3(176, 190, 214));
        shape(d, grip, f4(ccc3(44, 50, 70)));
        shape(d, barrel, steel);
        shape(d, body, metal);
        shape(d, rect(-9, 4.5f, 0, 6.8f), f4(ccc3(40, 220, 200)), .9f);
        fill(d, rect(-11, 1.2f, 0, 2.8f), f4(ccc3(110, 124, 156)));
        for (float x : {3.5f, 6.5f, 9.5f}) {
            d->drawSegment({x, -2.8f}, {x, 2.8f}, 1.2f, kInk);
            d->drawSegment({x, -2.6f}, {x, 2.6f}, .6f, f4(ccc3(120, 255, 235)));
        }
        glowDot(d, {14.f, 0}, 5.5f, ccc3(120, 255, 235), .9f);
        d->drawDot({14.f, 0}, 1.6f, white);
        break;
    }
    case 4: {
        medallion(d, ccc3(255, 120, 200));
        for (float a0 : {.4f, .4f + kPi})
            stroke(d, arc({0, 0}, 13.f, a0, a0 + 1.9f, 14), .7f, f4(ccc3(255, 160, 220), .85f), false);
        sword(d, ccc3(255, 120, 200), -42.f, 1.3f, {1.5f, -1.f});
        sword(d, ccc3(200, 120, 255), 42.f, 1.3f, {-1.5f, -1.f});
        break;
    }
    case 5: {
        medallion(d, ccc3(180, 120, 255));
        auto const violet = ccc3(196, 150, 255);
        stroke(d, circle({0, 0}, 12.4f, 40), .9f, f4(violet));
        stroke(d, circle({0, 0}, 10.6f, 40), .45f, f4(violet, .7f));
        Path t1, t2;
        for (int i = 0; i < 3; ++i) {
            float const a = kPi / 2 + i * 2 * kPi / 3;
            t1.push_back({std::cos(a) * 10.f, std::sin(a) * 10.f});
            t2.push_back({std::cos(a + kPi) * 10.f, std::sin(a + kPi) * 10.f});
        }
        stroke(d, t1, .6f, f4(ccc3(230, 210, 255)));
        stroke(d, t2, .6f, f4(ccc3(230, 210, 255)));
        for (int i = 0; i < 6; ++i) {
            float const a = kPi / 2 + i * kPi / 3;
            d->drawDot({std::cos(a) * 12.4f, std::sin(a) * 12.4f}, 1.3f, white);
        }
        glowDot(d, {0, 0}, 9.f, violet, .8f);
        Path gem{{0, 5.2f}, {3.6f, 0}, {0, -5.2f}, {-3.6f, 0}};
        shape(d, gem, f4(ccc3(150, 90, 255)), .9f);
        fill(d, {{0, 5.2f}, {-3.6f, 0}, {0, 0}}, f4(ccc3(220, 190, 255)));
        glint(d, {-1.2f, 2.2f}, 1.6f);
        break;
    }
    case 6:
        medallion(d, ccc3(255, 140, 40));
        glowDot(d, {0, 0}, 13.f, ccc3(255, 190, 80), .8f);
        break;
    case 7: {
        medallion(d, ccc3(230, 220, 170));
        auto const gold = ccc3(255, 214, 110);
        for (int i = 0; i < 12; ++i) {
            float const a = i * kPi / 6.f + .26f, r = i % 2 ? 13.f : 15.f;
            fill(d,
                 {{std::cos(a - .16f) * 8.f, std::sin(a - .16f) * 8.f},
                  {std::cos(a) * r, std::sin(a) * r},
                  {std::cos(a + .16f) * 8.f, std::sin(a + .16f) * 8.f}},
                 f4(gold, .9f));
        }
        disc(d, {0, 0}, 9.f, f4(gold));
        d->drawDot({0, 0}, 9.8f, f4(ccc3(255, 248, 225), .5f));
        disc(d, {2.2f, 1.6f}, 8.4f, f4(ccc3(18, 18, 34)), .6f);
        stroke(d, arc({2.2f, 1.6f}, 8.f, 2.3f, 3.9f, 10), .4f, {.75f, .8f, .95f, .8f}, false);
        d->drawSegment({-14.4f, -12.f}, {14.4f, 12.f}, .75f, f4(ccc3(200, 225, 255), .45f));
        d->drawSegment({-14.7f, -12.25f}, {14.7f, 12.25f}, .26f, white);
        break;
    }
    case 8: {
        medallion(d, ccc3(120, 230, 140));
        glowDot(d, {-2, 0}, 12.f, ccc3(170, 255, 190), .5f);
        for (auto [x, y, r] :
             {std::tuple{-11.f, -9.f, 1.6f}, std::tuple{-8.f, -12.5f, 1.2f}, std::tuple{-13.f, -4.5f, 1.1f}})
            fill(d, rect(x - r, y - r, x + r, y + r), f4(ccc3(255, 150, 190)));
        break;
    }
    case 9: {
        medallion(d, ccc3(255, 150, 70));
        d->drawDot({0, -2.f}, 15.6f, f4(ccc3(255, 120, 60), .55f));
        glowDot(d, {0, -3.f}, 11.f, ccc3(255, 200, 110), .8f);
        disc(d, {0, -3.f}, 7.f, f4(ccc3(255, 214, 120)), .0f);
        fill(d, rect(-16.f, -16.f, 16.f, -3.f), f4(ccc3(70, 24, 40)));
        stroke(d, {{-16.f, -3.f}, {16.f, -3.f}}, .5f, f4(ccc3(255, 170, 90)), false);
        break;
    }
    }
}

// 스프라이트 시트 프레임 / 번들 이미지(너비 기준 크기 조절)로 스프라이트를 만든다.
inline CCSprite* sheetSprite(char const* name, char const* sheet) {
    return versus::loadedFrameSprite(name, sheet);
}
inline CCSprite* bundledSprite(char const* file, float width) {
    auto const resource = std::string(geode::Mod::get()->expandSpriteName(file));
    auto* sprite = versus::fileSprite(resource.c_str());
    if (sprite) sprite->setScale(width / std::max(1.f, sprite->getContentSize().width));
    return sprite;
}

// 피니셔 아이콘 노드. 벡터 그림 위에 Monster Chomp(8)은 GD 시트의 괴수 스프라이트,
// Sunset Showdown(9)은 번들 이미지(리볼버, 모자), Phoenix Judgment(6)는 Phoenix 그림을
// 덧붙인다.
inline CCNode* finisherIcon(int kind) {
    auto* holder = CCNode::create();
    auto* d = CCDrawNode::create();
    finisherArt(d, kind);
    holder->addChild(d);
    if (kind == 8) {
        auto* head = sheetSprite("GJBeast01_01_001.png", "FireSheet_01.plist");
        auto* jaw = sheetSprite("GJBeast01_02_001.png", "FireSheet_01.plist");
        if (head && jaw) {
            auto* beast = CCNode::create();
            beast->setScale(.36f);
            beast->setPosition({3.f, 1.f});
            for (auto [part, at, turn] :
                 {std::tuple{jaw, CCPoint{8.f, -19.f}, -27.f}, std::tuple{head, CCPoint{6.8f, 20.f}, 29.f}}) {
                part->setColor(ccc3(12, 10, 18));
                part->setPosition(at);
                part->setRotation(turn);
                beast->addChild(part);
            }
            holder->addChild(beast);
        }
    }
    if (kind == 9) {
        if (auto* gun = bundledSprite("lock-on-revolver.png", 25.f)) {
            gun->setPosition({3.f, -4.f});
            gun->setRotation(-8.f);
            holder->addChild(gun);
        }
        if (auto* hat = bundledSprite("lock-on-hat.png", 21.f)) {
            hat->setPosition({-5.f, 9.f});
            hat->setRotation(-12.f);
            holder->addChild(hat);
        }
    }
    if (kind == 6) {
        auto* bird = CCNode::create();
        bird->setScale(.7f);
        bird->setPosition({4.f, -4.f});
        PhoenixColors c;
        auto* backWing = CCDrawNode::create();
        phoenixWing(backWing, PhoenixColors{dark(c.primary, .25f), dark(c.secondary, .25f), dark(c.deep, .25f)});
        backWing->setPosition(phoenixShoulder() + CCPoint{3.f, 1.5f});
        backWing->setScale(.85f);
        bird->addChild(backWing);
        auto* body = CCDrawNode::create();
        phoenixBody(body, c);
        bird->addChild(body);
        auto* wing = CCDrawNode::create();
        phoenixWing(wing, c);
        wing->setPosition(phoenixShoulder());
        bird->addChild(wing);
        holder->addChild(bird);
    }
    return holder;
}
}
