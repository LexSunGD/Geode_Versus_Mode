// 처형(finisher) 연출 구현. 이긴 쪽(winner)이 진 쪽(loser)에게 가하는 종류별 컷신을 만든다.
// 연출은 전부 코드로 그린다 (CCDrawNode 도형, 파티클, 일부 이미지). play()가 kind(1~9)에 맞는 함수를
// 고르고, 각 함수는 충돌 시점(impact, 초)을 돌려주면 공통 후처리(loser 날려 보내기, 화면 흔들림, 섬광,
// 충격 효과, 효과음)가 그 시점에 맞춰 붙는다. 모든 시각은 stage 노드의 액션으로 예약된다.
#include "ExecutionEffects.hpp"
#include "Particles.hpp"
#include "VersusAudio.hpp"
#include "VectorArt.hpp"
#include "SpriteFrames.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/CCCircleWave.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <vector>
using namespace geode::prelude;

namespace versus::execution {
namespace {
// 2π, 색 변환(알파 포함), 극좌표 -> 좌표 도우미.
constexpr float kTau = 6.2831853f;

ccColor4F rgba(ccColor3B c, float a) {
    return {c.r / 255.f, c.g / 255.f, c.b / 255.f, a};
}
CCPoint polar(float radius, float angle) {
    return {std::cos(angle) * radius, std::sin(angle) * radius};
}

// CCDrawNode로 그리는 빛 번짐 노드. 투명도가 바뀔 때마다 paint 함수로 다시 그려서
// 페이드/스케일 액션만으로 도형이 자연스럽게 사라지게 한다 (같은 투명도면 다시 그리지 않음).
class Glow : public CCNodeRGBA {
    CCDrawNode* m_draw = nullptr;
    std::function<void(CCDrawNode*, float)> m_paint;
    GLubyte m_painted = 0;
    bool m_valid = false;

    void repaint() {
        GLubyte const value = getDisplayedOpacity();
        if (m_valid && value == m_painted) return;
        m_draw->clear();
        if (value) m_paint(m_draw, value / 255.f);
        m_painted = value;
        m_valid = true;
    }

public:
    static Glow* create(std::function<void(CCDrawNode*, float)> paint, bool additive = true) {
        auto* result = new Glow;
        if (!result->init()) {
            delete result;
            return nullptr;
        }
        result->autorelease();
        result->m_draw = CCDrawNode::create();
        if (additive) result->m_draw->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        result->addChild(result->m_draw);
        result->m_paint = std::move(paint);
        result->setCascadeOpacityEnabled(false);
        result->repaint();
        return result;
    }
    void setOpacity(GLubyte value) override {
        CCNodeRGBA::setOpacity(value);
        repaint();
    }
    void updateDisplayedOpacity(GLubyte parent) override {
        CCNodeRGBA::updateDisplayedOpacity(parent);
        repaint();
    }
};

// 그리기 도우미: 원 테두리 / 가운데가 밝은 둥근 빛 / 가는 십자 모양 별 / Glow를 만들어 parent에 붙이기.
void ring(CCDrawNode* draw, float radius, float width, ccColor4F color, int segments = 40) {
    for (int i = 0; i < segments; ++i)
        draw->drawSegment(polar(radius, i * kTau / segments), polar(radius, (i + 1) * kTau / segments), width, color);
}

void softDot(CCDrawNode* draw, CCPoint at, float radius, ccColor3B tint, float alpha) {
    draw->drawDot(at, radius, rgba(tint, alpha * .16f));
    draw->drawDot(at, radius * .62f, rgba(tint, alpha * .28f));
    draw->drawDot(at, radius * .32f, rgba(tint, alpha * .55f));
    draw->drawDot(at, radius * .14f, {1, 1, 1, alpha * .9f});
}

void star(CCDrawNode* draw, float length, float width, ccColor3B tint, float alpha, int points = 4) {
    for (int i = 0; i < points; ++i) {
        float const angle = i * kTau / points;
        float const reach = i % 2 ? length * .55f : length;
        draw->drawSegment(polar(-reach, angle), polar(reach, angle), width, rgba(tint, alpha * .55f));
        draw->drawSegment(polar(-reach * .7f, angle), polar(reach * .7f, angle), width * .35f, {1, 1, 1, alpha});
    }
    softDot(draw, {}, length * .45f, tint, alpha);
}

Glow* addGlow(CCNode* parent, CCPoint at, int z, std::function<void(CCDrawNode*, float)> paint) {
    auto* glow = Glow::create(std::move(paint));
    glow->setPosition(at);
    parent->addChild(glow, z);
    return glow;
}

// 순간적으로 커지며 회전하고 사라지는 별 모양 섬광.
void flare(CCNode* parent, CCPoint at, ccColor3B tint, float size, float life, int z = 14, int points = 4) {
    auto* glow =
        addGlow(parent, at, z, [size, tint, points](CCDrawNode* d, float a) { star(d, size, 2.2f, tint, a, points); });
    glow->setScale(.35f);
    glow->setRotation(static_cast<float>(std::rand() % 90));
    glow->runAction(
        CCSequence::create(CCSpawn::create(CCEaseExponentialOut::create(CCScaleTo::create(life, 1.25f)),
                                           CCRotateBy::create(life, 40.f), CCFadeOut::create(life), nullptr),
                           CCRemoveSelf::create(), nullptr));
}

// GD 파티클 plist(preset)를 짧게 한 번 터뜨린다. 개수/지속/수명/속도/크기를 덮어쓴다.
// 파티클이 막힌 환경에서는 nullptr를 돌려주므로 호출부에서 확인해야 한다.
CCParticleSystemQuad* nativeBurst(CCNode* parent, CCPoint position, char const* preset, ccColor3B tint, unsigned count,
                                  float duration, float life, float speed, float size) {
    auto* particles = versus::particleSystem(preset);
    if (!particles) return nullptr;
    particles->setTotalParticles(count);
    particles->setEmitterMode(kCCParticleModeGravity);
    particles->setPositionType(kCCPositionTypeGrouped);
    particles->setSourcePosition({0.f, 0.f});
    particles->setPosition(position);
    particles->setPosVar({4.f, 4.f});
    particles->setDuration(duration);
    particles->setLife(life);
    particles->setLifeVar(life * .25f);
    particles->setEmissionRate(static_cast<float>(count) / duration);
    particles->setAngle(90.f);
    particles->setAngleVar(180.f);
    particles->setSpeed(speed);
    particles->setSpeedVar(speed * .3f);
    particles->setGravity({0.f, -35.f});
    particles->setStartSize(size);
    particles->setStartSizeVar(size * .3f);
    particles->setEndSize(0.f);
    particles->setEndSizeVar(0.f);
    particles->setStartColor({tint.r / 255.f, tint.g / 255.f, tint.b / 255.f, 1.f});
    particles->setStartColorVar({.08f, .08f, .08f, 0.f});
    particles->setEndColor({tint.r / 255.f, tint.g / 255.f, tint.b / 255.f, 0.f});
    particles->setEndColorVar({0.f, 0.f, 0.f, 0.f});
    particles->setBlendAdditive(true);
    particles->setAutoRemoveOnFinish(true);
    parent->addChild(particles, 16);
    particles->resetSystem();
    return particles;
}

// 바깥에서 중심으로 모여들며 도는 파티클 (충전하는 느낌).
void convergeBurst(CCNode* parent, CCPoint position, ccColor3B tint, unsigned count, float duration, float radius) {
    auto* particles = nativeBurst(parent, position, "glitterEffectIcon.plist", tint, count, duration, .45f, 0.f, 6.f);
    if (!particles) return;
    particles->setEmitterMode(kCCParticleModeRadius);
    particles->setStartRadius(radius);
    particles->setStartRadiusVar(radius * .2f);
    particles->setEndRadius(0.f);
    particles->setEndRadiusVar(0.f);
    particles->setRotatePerSecond(220.f);
    particles->setRotatePerSecondVar(60.f);
    particles->setAngleVar(180.f);
}

// GD 기본 원형 충격파(CCCircleWave).
void nativeWave(CCNode* parent, CCPoint position, ccColor3B tint, float radius) {
    if (auto* wave = CCCircleWave::create(8.f, radius, .4f, false, true)) {
        wave->m_color = tint;
        wave->setPosition(position);
        parent->addChild(wave, 15);
    }
}

// 공통 충돌 효과: 충격파, 파티클 폭발, 섬광, 잠시 뒤 두 번째 충격파.
void impactEffect(CCNode* parent, CCPoint position, ccColor3B tint) {
    nativeWave(parent, position, ccWHITE, 80.f);
    nativeBurst(parent, position, "explodeEffect.plist", tint, 64, .06f, .48f, 185.f, 7.f);
    nativeBurst(parent, position, "glitterEffectIcon.plist", ccWHITE, 20, .08f, .4f, 110.f, 9.f);
    flare(parent, position, tint, 46.f, .38f, 17, 8);
    parent->runAction(CCSequence::create(
        CCDelayTime::create(.09f),
        CallFuncExt::create([parent, position, tint] { nativeWave(parent, position, tint, 58.f); }), nullptr));
}

// 날아가는 투사체 뒤에 남는 꼬리 파티클.
void projectileTrail(CCNode* shot, float direction, ccColor3B tint) {
    if (auto* particles = nativeBurst(shot, {0.f, 0.f}, "fireballEffect.plist", tint, 32, .44f, .2f, 35.f, 5.f)) {
        particles->setPositionType(kCCPositionTypeFree);
        particles->setAngle(direction > 0.f ? 180.f : 0.f);
        particles->setAngleVar(25.f);
        particles->setGravity({0.f, 0.f});
        particles->setZOrder(-1);
    }
}

// 프레임 이름으로 투사체 스프라이트를 만든다 (없으면 사각형). 긴 변을 26px로 맞춘다.
CCSprite* projectileSprite(char const* frame, ccColor3B tint) {
    auto* sprite = loadedFrameSprite(frame);
    if (!sprite) sprite = CCSprite::create("square.png");
    sprite->setColor(tint);
    auto const size = sprite->getContentSize();
    sprite->setScale(26.f / std::max(1.f, std::max(size.width, size.height)));
    return sprite;
}

// from에서 to까지 뻗는 광선. 얇아지며 사라진다.
Glow* beam(CCNode* parent, CCPoint from, CCPoint to, ccColor3B tint, float width, float life, int z = 12) {
    auto const delta = to - from;
    float const length = std::max(1.f, std::sqrt(delta.x * delta.x + delta.y * delta.y));
    auto* ray = addGlow(parent, from, z, [length, width, tint](CCDrawNode* d, float a) {
        d->drawSegment({0, 0}, {length, 0}, width, rgba(tint, a * .22f));
        d->drawSegment({0, 0}, {length, 0}, width * .55f, rgba(tint, a * .55f));
        d->drawSegment({0, 0}, {length, 0}, width * .2f, {1, 1, 1, a});
    });
    ray->setRotation(-std::atan2(delta.y, delta.x) * 57.29578f);
    ray->runAction(CCSequence::create(
        CCSpawn::create(CCEaseSineIn::create(CCScaleTo::create(life, 1.f, .05f)), CCFadeOut::create(life), nullptr),
        CCRemoveSelf::create(), nullptr));
    return ray;
}

// 호 모양의 칼자국. 커지며 살짝 회전하고 사라진다.
void slash(CCNode* parent, CCPoint at, float rotation, ccColor3B tint, float scale = 1.f) {
    auto* arc = addGlow(parent, at, 13, [tint](CCDrawNode* d, float a) {
        for (int i = 0; i < 22; ++i) {
            float const s = -1.25f + i * .114f, e = s + .114f;
            CCPoint const p{std::cos(s) * 62.f, std::sin(s) * 40.f}, q{std::cos(e) * 62.f, std::sin(e) * 40.f};
            float const w = .8f + 5.2f * std::sin((i + 1) / 23.f * 3.14159f);
            d->drawSegment(p, q, w * 1.9f, rgba(tint, a * .25f));
            d->drawSegment(p, q, w, rgba(tint, a * .8f));
            d->drawSegment(p, q, w * .32f, {1, 1, 1, a});
        }
    });
    arc->setRotation(rotation);
    arc->setScale(.55f * scale);
    arc->runAction(CCSequence::create(
        CCSpawn::create(CCEaseExponentialOut::create(CCScaleTo::create(.3f, 1.35f * scale)),
                        CCRotateBy::create(.3f, rotation > 0 ? -18.f : 18.f), CCFadeOut::create(.3f), nullptr),
        CCRemoveSelf::create(), nullptr));
}

// 마법진 문양 (이중 원, 육각 별, 눈금, 중심 빛).
Glow* sigil(CCNode* parent, CCPoint at, float radius, ccColor3B tint, int z) {
    return addGlow(parent, at, z, [radius, tint](CCDrawNode* d, float a) {
        auto const c = rgba(tint, a * .85f);
        ring(d, radius, 1.1f, c, 48);
        ring(d, radius * .9f, .6f, rgba(tint, a * .5f), 48);
        for (int i = 0; i < 6; ++i) {
            float const s = i * kTau / 6.f, e = (i + 2) * kTau / 6.f;
            d->drawSegment(polar(radius * .86f, s), polar(radius * .86f, e), .8f, c);
            d->drawDot(polar(radius, s), 2.4f, {1, 1, 1, a});
        }
        for (int i = 0; i < 24; ++i) {
            float const t = i * kTau / 24.f + .13f;
            d->drawSegment(polar(radius * .93f, t), polar(radius * (i % 3 ? .96f : 1.04f), t), .6f, c);
        }
        ring(d, radius * .38f, .7f, c, 24);
        softDot(d, {}, radius * .55f, tint, a * .5f);
    });
}

// node를 좌우로 짧게 떨게 한다. 이동한 만큼 되돌려 원래 위치로 돌아온다.
void shake(CCNode* node, float duration, float amplitude) {
    int const steps = std::max(2, static_cast<int>(duration / .04f));
    auto* actions = CCArray::create();
    for (int i = 0; i < steps; ++i) {
        float const side = i % 2 ? -1.f : 1.f;
        actions->addObject(CCMoveBy::create(duration / steps / 2.f, {side * amplitude, (i % 3 - 1) * amplitude * .5f}));
        actions->addObject(
            CCMoveBy::create(duration / steps / 2.f, {-side * amplitude, -(i % 3 - 1) * amplitude * .5f}));
    }
    node->runAction(CCSequence::create(actions));
}

// root에 delay초 뒤 callback을 실행하도록 예약한다. 이 파일의 시간 연출은 거의 이것으로 짠다.
template <class F> void at(CCNode* root, float delay, F&& callback) {
    root->runAction(CCSequence::create(CCDelayTime::create(std::max(0.f, delay)),
                                       CallFuncExt::create(std::forward<F>(callback)), nullptr));
}

// kind 1 (기본): 가시 세 개를 쏜다. winner는 뒤로 공중제비를 돈다. 충돌 시점을 돌려준다.
float spikes(CCNode* actors, CCNode* winner, float direction, float t0) {
    for (int i = -1; i <= 1; ++i) {
        auto* shot = CCNode::create();
        auto* spike = projectileSprite("spike_01_001.png", ccc3(144, 224, 255));
        spike->setRotation(direction * 90.f);
        shot->addChild(spike);
        shot->setPosition(winner->getPosition() + CCPoint{0.f, i * 16.f});
        shot->setScale(.35f);
        shot->setVisible(false);
        actors->addChild(shot, 4);
        shot->runAction(CCSequence::create(
            CCDelayTime::create(t0 + (i + 1) * .07f), CCShow::create(),
            CallFuncExt::create([shot, direction] { projectileTrail(shot, direction, ccc3(126, 217, 255)); }),
            CCSpawn::create(CCEaseSineIn::create(CCMoveBy::create(.44f, {direction * 190.f, 0.f})),
                            CCEaseBackOut::create(CCScaleTo::create(.25f, 1.f)), nullptr),
            CCRemoveSelf::create(), nullptr));
    }
    winner->runAction(CCSequence::create(CCDelayTime::create(t0 - .30f),
                                         CCSpawn::create(CCJumpBy::create(.58f, {direction * 34.f, 0.f}, 30.f, 1),
                                                         CCRotateBy::create(.58f, -direction * 360.f), nullptr),
                                         CCEaseSineOut::create(CCRotateTo::create(.25f, 0.f)), nullptr));
    for (int i = 0; i < 3; ++i)
        at(actors, t0 + i * .07f, [i] { audio::fx(i == 1 ? "fx_knife2" : "fx_knife1", "playSound_01.ogg"); });
    return t0 + .58f;
}

// kind 2: 화염구를 쏜다. winner는 뒤로 젖혔다 내민다.
float fireball(CCNode* actors, CCNode* winner, float direction, float t0) {
    auto* shot = CCNode::create();
    shot->addChild(projectileSprite("fireball_01_001.png", ccc3(255, 193, 82)));
    shot->setPosition(winner->getPosition());
    shot->setScale(.2f);
    shot->setVisible(false);
    actors->addChild(shot, 4);
    shot->runAction(CCSequence::create(
        CCDelayTime::create(t0), CCShow::create(),
        CallFuncExt::create([shot, direction] { projectileTrail(shot, direction, ccc3(255, 157, 65)); }),
        CCSpawn::create(CCEaseSineIn::create(CCMoveBy::create(.48f, {direction * 190.f, 0.f})),
                        CCEaseBackOut::create(CCScaleTo::create(.30f, 1.25f)), nullptr),
        CCRemoveSelf::create(), nullptr));
    winner->runAction(CCSequence::create(CCDelayTime::create(t0 - .28f),
                                         CCSpawn::create(CCRotateTo::create(.28f, -direction * 22.f),
                                                         CCMoveBy::create(.28f, {-direction * 18.f, 0.f}), nullptr),
                                         CCSpawn::create(CCRotateTo::create(.42f, direction * 8.f),
                                                         CCMoveBy::create(.42f, {direction * 15.f, 0.f}), nullptr),
                                         CCEaseSineOut::create(CCRotateTo::create(.38f, 0.f)), nullptr));
    at(actors, t0, [] { audio::fx("fx_flame", "playSound_01.ogg"); });
    return t0 + .48f;
}

// kind 3: 레일건. 총 등장 -> 충전(수렴 파티클, 링) -> 조준선 점멸 -> 발사(광선, 충격파, 탄피) 순서.
float railgun(CCNode* actors, CCNode* winner, CCNode* loser, float direction, float t0) {
    ccColor3B const cyan = ccc3(80, 255, 225);
    auto const holder = winner->getPosition() + CCPoint{direction * 22.f, 3.f};
    auto* gun = Glow::create(
        [](CCDrawNode* d, float a) {
            ccColor4F const body{.07f * a, .12f * a, .2f * a, a}, plate{.16f, .26f, .38f, a};
            ccColor4F const coil{.25f, 1.f, .86f, a}, hot{.85f, 1.f, 1.f, a};
            CCPoint frame[] = {{-20, -5}, {30, -5}, {36, 0}, {30, 6}, {-20, 6}};
            d->drawPolygon(frame, 5, body, 1.f, plate);
            CCPoint grip[] = {{-12, -5}, {-5, -5}, {-10, -20}, {-17, -20}};
            d->drawPolygon(grip, 4, plate, .6f, body);
            d->drawSegment({-8, 9}, {44, 9}, 1.6f, coil);
            d->drawSegment({-8, -8}, {44, -8}, 1.6f, coil);
            for (int i = 0; i < 4; ++i)
                d->drawSegment({2.f + i * 10.f, -8}, {2.f + i * 10.f, 9}, .9f, {.3f, .9f, 1.f, a * .7f});
            d->drawSegment({36, 0}, {47, 0}, 2.6f, hot);
            d->drawDot({-14, 1}, 2.f, coil);
        },
        false);
    gun->setPosition(holder);
    gun->setScaleX(0.f);
    gun->setScaleY(0.f);
    actors->addChild(gun, 8);
    gun->runAction(CCSequence::create(
        CCDelayTime::create(t0 - .3f), CCEaseBackOut::create(CCScaleTo::create(.22f, direction, 1.f)),
        CCMoveBy::create(.16f, {direction * 6.f, 0.f}), CCDelayTime::create(.62f),
        CCMoveBy::create(.05f, {-direction * 15.f, 0.f}),
        CCEaseSineOut::create(CCMoveBy::create(.3f, {direction * 9.f, 0.f})), CCDelayTime::create(.3f),
        CCFadeOut::create(.22f), CCRemoveSelf::create(), nullptr));
    auto const muzzle = holder + CCPoint{direction * 53.f, 3.f};
    auto const target = loser->getPosition();
    float const fire = t0 + .82f;

    at(actors, t0 - .3f, [actors, holder, cyan] {
        flare(actors, holder, cyan, 24.f, .3f, 9);
        audio::fx("fx_reload", "unlockPath.ogg");
    });
    at(actors, t0, [actors, muzzle, cyan] {
        audio::fx("fx_charge");
        convergeBurst(actors, muzzle, cyan, 36, .7f, 46.f);
        for (int i = 0; i < 3; ++i) {
            auto* rim =
                addGlow(actors, muzzle, 9, [cyan](CCDrawNode* d, float a) { ring(d, 22.f, 1.2f, rgba(cyan, a), 28); });
            rim->setScale(2.2f);
            rim->setOpacity(0);
            rim->runAction(CCSequence::create(CCDelayTime::create(i * .2f),
                                              CCSpawn::create(CCEaseSineIn::create(CCScaleTo::create(.32f, .15f)),
                                                              CCFadeTo::create(.12f, 255), nullptr),
                                              CCRemoveSelf::create(), nullptr));
        }
        auto* core = addGlow(actors, muzzle, 10, [cyan](CCDrawNode* d, float a) { softDot(d, {}, 16.f, cyan, a); });
        core->setScale(.1f);
        core->runAction(
            CCSequence::create(CCEaseSineIn::create(CCScaleTo::create(.8f, 1.25f)),
                               CCSpawn::create(CCScaleTo::create(.12f, 2.2f), CCFadeOut::create(.12f), nullptr),
                               CCRemoveSelf::create(), nullptr));
    });
    at(actors, t0 + .12f, [actors, muzzle, target] {
        auto const delta = target - muzzle;
        float const length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        auto* sight = addGlow(actors, muzzle, 7, [length](CCDrawNode* d, float a) {
            d->drawSegment({0, 0}, {length, 0}, .7f, {1.f, .25f, .3f, a * .8f});
            d->drawDot({length, 0}, 3.f, {1.f, .3f, .35f, a});
        });
        sight->setRotation(-std::atan2(delta.y, delta.x) * 57.29578f);
        sight->runAction(CCSequence::create(CCBlink::create(.6f, 5), CCRemoveSelf::create(), nullptr));
    });
    at(actors, fire, [actors, muzzle, target, cyan, direction] {
        beam(actors, muzzle, target, cyan, 15.f, .42f);
        beam(actors, muzzle, target + CCPoint{direction * 150.f, 0.f}, ccWHITE, 4.f, .2f, 13);
        flare(actors, muzzle, ccWHITE, 34.f, .25f, 15, 6);
        nativeWave(actors, muzzle, ccWHITE, 42.f);
        for (int i = 1; i <= 3; ++i) {
            auto const point = muzzle + (target - muzzle) * (i / 4.f);
            at(actors, i * .04f, [actors, point, cyan] { nativeWave(actors, point, cyan, 24.f); });
        }
        audio::fx("fx_laser", "explode_11.ogg");
        auto* casing = addGlow(actors, muzzle - CCPoint{direction * 40.f, -6.f}, 9, [](CCDrawNode* d, float a) {
            d->drawSegment({-3, 0}, {3, 0}, 1.6f, {1.f, .82f, .35f, a});
        });
        casing->runAction(CCSequence::create(CCSpawn::create(CCJumpBy::create(.5f, {-direction * 34.f, -26.f}, 24.f, 1),
                                                             CCRotateBy::create(.5f, -direction * 720.f), nullptr),
                                             CCFadeOut::create(.15f), CCRemoveSelf::create(), nullptr));
    });
    winner->runAction(CCSequence::create(CCDelayTime::create(fire), CCMoveBy::create(.05f, {-direction * 16.f, 0.f}),
                                         CCEaseSineOut::create(CCMoveBy::create(.32f, {direction * 16.f, 0.f})),
                                         nullptr));
    return fire;
}

// kind 4: 검. 검을 뽑고 winner가 loser에게 돌진해 세 번 베고, 마지막에 교차 참격으로 마무리한다.
float blade(CCNode* actors, CCNode* winner, CCNode* loser, float direction, float t0) {
    ccColor3B const pink = ccc3(255, 105, 190);
    auto const home = winner->getPosition();
    auto const target = loser->getPosition();
    auto* sword = Glow::create(
        [](CCDrawNode* d, float a) {
            CCPoint edge[] = {{-3.2f, 0}, {3.2f, 0}, {4.6f, 48}, {0, 64}, {-4.6f, 48}};
            d->drawPolygon(edge, 5, {.86f, .96f, 1.f, a}, .9f, {1.f, .45f, .78f, a});
            d->drawSegment({0, 4}, {0, 56}, .5f, {1.f, 1.f, 1.f, a});
            d->drawSegment({-12, 0}, {12, 0}, 2.2f, {1.f, .72f, .25f, a});
            d->drawSegment({0, -16}, {0, -1}, 2.6f, {.22f, .14f, .32f, a});
            d->drawDot({0, -18}, 2.6f, {1.f, .72f, .25f, a});
        },
        false);
    sword->setPosition(home + CCPoint{direction * 16.f, 12.f});
    sword->setRotation(direction * 30.f);
    sword->setScale(0.f);
    actors->addChild(sword, 8);
    sword->runAction(CCSequence::create(
        CCDelayTime::create(t0 - .32f), CCEaseBackOut::create(CCScaleTo::create(.22f, .9f)),
        CCEaseSineInOut::create(CCRotateBy::create(.3f, -direction * 75.f)),
        CCMoveTo::create(.1f, target + CCPoint{direction * 70.f, 14.f}), CCRotateBy::create(.7f, direction * 50.f),
        CCSpawn::create(CCRotateBy::create(.25f, direction * 200.f), CCFadeOut::create(.25f), nullptr),
        CCRemoveSelf::create(), nullptr));
    at(actors, t0 - .1f, [actors, home, direction] {
        flare(actors, home + CCPoint{direction * 16.f, 70.f}, ccWHITE, 20.f, .35f, 14);
        audio::fx("fx_unsheathe", "playSound_01.ogg");
    });
    float const dash = t0 + .2f;
    winner->runAction(
        CCSequence::create(CCDelayTime::create(dash),
                           CCEaseExponentialIn::create(CCMoveTo::create(.1f, target + CCPoint{direction * 70.f, 0.f})),
                           CCDelayTime::create(.95f), CCEaseSineInOut::create(CCMoveTo::create(.35f, home)), nullptr));
    at(actors, dash + .06f, [actors, home, target, pink, direction] {
        for (int i = 0; i < 5; ++i) {
            float const y = (i - 2) * 7.f;
            auto const start = home + CCPoint{0.f, y};
            auto* streak = addGlow(
                actors, start, 6, [length = std::abs(target.x - home.x) + 70.f, pink, i](CCDrawNode* d, float a) {
                    d->drawSegment({0, 0}, {length, 0}, i == 2 ? 2.2f : 1.f, rgba(i == 2 ? ccWHITE : pink, a * .8f));
                });
            streak->setScaleX(direction);
            streak->runAction(CCSequence::create(
                CCSpawn::create(CCScaleTo::create(.32f, direction, .1f), CCFadeOut::create(.32f), nullptr),
                CCRemoveSelf::create(), nullptr));
        }
        nativeWave(actors, home, pink, 36.f);
        audio::fx("fx_hoon", "playSound_01.ogg");
    });
    for (int cut = 0; cut < 3; ++cut) {
        at(actors, dash + .2f + cut * .17f, [actors, loser, target, pink, cut] {
            float const angle = cut == 0 ? 32.f : cut == 1 ? -38.f : 180.f + 12.f;
            slash(actors, target, angle, cut == 1 ? ccc3(180, 120, 255) : pink, 1.f + cut * .12f);
            nativeBurst(actors, target, "glitterEffectIcon.plist", ccWHITE, 10, .05f, .25f, 140.f, 5.f);
            shake(loser, .12f, 3.f);
            audio::fx("fx_cut", "playSound_01.ogg");
        });
    }
    float const finale = dash + .78f;
    at(actors, finale, [actors, target, pink] {
        for (int i = 0; i < 2; ++i) {
            auto* line = addGlow(actors, target, 18, [pink](CCDrawNode* d, float a) {
                d->drawSegment({-230, 0}, {230, 0}, 7.f, rgba(pink, a * .35f));
                d->drawSegment({-230, 0}, {230, 0}, 2.4f, rgba(pink, a));
                d->drawSegment({-230, 0}, {230, 0}, .9f, {1, 1, 1, a});
            });
            line->setRotation(i ? -34.f : 34.f);
            line->setScaleX(.02f);
            line->runAction(CCSequence::create(
                CCDelayTime::create(i * .08f), CCEaseExponentialOut::create(CCScaleTo::create(.14f, 1.f, 1.f)),
                CCDelayTime::create(.18f),
                CCSpawn::create(CCScaleTo::create(.22f, 1.f, .05f), CCFadeOut::create(.22f), nullptr),
                CCRemoveSelf::create(), nullptr));
        }
        flare(actors, target, ccWHITE, 40.f, .3f, 19, 8);
        audio::fx("fx_ching");
    });
    return finale + .32f;
}

// kind 5: 마법. 발밑 마법진, 구체 세 개, loser 위의 마법진에서 구체를 쏘고 마지막에 빛기둥이 내려친다.
float arcane(CCNode* actors, CCNode* winner, CCNode* loser, float direction, float t0) {
    ccColor3B const violet = ccc3(200, 120, 255), azure = ccc3(120, 175, 255);
    auto const caster = winner->getPosition();
    auto const target = loser->getPosition();
    auto* ground = CCNode::create();
    ground->setPosition(caster - CCPoint{0.f, 20.f});
    ground->setScaleX(0.f);
    ground->setScaleY(0.f);
    actors->addChild(ground, -1);
    auto* own = sigil(ground, {}, 28.f, azure, 0);
    ground->runAction(CCSequence::create(
        CCDelayTime::create(t0 - .3f), CCEaseBackOut::create(CCScaleTo::create(.3f, 1.f, .32f)),
        CCDelayTime::create(1.9f), CCScaleTo::create(.3f, 0.f, 0.f), CCRemoveSelf::create(), nullptr));
    own->runAction(CCSequence::create(CCDelayTime::create(t0 - .3f),
                                      CCEaseSineInOut::create(CCRotateBy::create(2.2f, -direction * 150.f)),
                                      CCFadeOut::create(.3f), nullptr));
    winner->runAction(CCSequence::create(
        CCDelayTime::create(t0 - .3f), CCEaseSineOut::create(CCMoveBy::create(.45f, {0.f, 12.f})),
        CCDelayTime::create(1.5f), CCEaseSineInOut::create(CCMoveBy::create(.4f, {0.f, -12.f})), nullptr));
    at(actors, t0 - .3f, [] { audio::fx("fx_spell", "unlockPath.ogg"); });

    auto* hub = CCNode::create();
    hub->setPosition(caster + CCPoint{0.f, 12.f});
    actors->addChild(hub, 9);
    std::array<Glow*, 3> orbs{};
    for (int i = 0; i < 3; ++i) {
        orbs[i] = addGlow(hub, polar(30.f, i * kTau / 3.f), 0,
                          [violet](CCDrawNode* d, float a) { softDot(d, {}, 11.f, violet, a); });
        orbs[i]->setScale(0.f);
        orbs[i]->runAction(CCSequence::create(CCDelayTime::create(t0 - .1f + i * .08f),
                                              CCEaseBackOut::create(CCScaleTo::create(.2f, 1.f)), nullptr));
    }
    hub->runAction(
        CCSequence::create(CCDelayTime::create(t0 - .1f), CCRotateBy::create(1.2f, -direction * 540.f), nullptr));

    for (int layer = 0; layer < 2; ++layer) {
        auto* seal = sigil(actors, target, layer ? 52.f : 36.f, layer ? azure : violet, layer ? -1 : 7);
        seal->setScale(.1f);
        seal->setOpacity(0);
        seal->runAction(CCSequence::create(
            CCDelayTime::create(t0 + .05f),
            CCSpawn::create(CCEaseBackOut::create(CCScaleTo::create(.45f, 1.f)), CCFadeIn::create(.2f), nullptr),
            CCRotateBy::create(1.25f, layer ? -170.f : 240.f),
            CCSpawn::create(CCEaseSineIn::create(CCScaleTo::create(.3f, .05f)), CCFadeOut::create(.3f), nullptr),
            CCRemoveSelf::create(), nullptr));
    }
    loser->runAction(CCSequence::create(CCDelayTime::create(t0 + .1f),
                                        CCEaseSineOut::create(CCMoveBy::create(.7f, {0.f, 20.f})), nullptr));
    at(actors, t0 + .55f, [loser] { shake(loser, .75f, 1.6f); });
    at(actors, t0 + .1f, [actors, target, violet] {
        nativeBurst(actors, target, "glitterEffectIcon.plist", violet, 26, .8f, .55f, 26.f, 7.f);
        audio::fx("fx_seal");
        audio::fx("fx_orbit");
    });

    for (int i = 0; i < 3; ++i) {
        float const launch = t0 + .55f + i * .14f;
        at(actors, launch, [actors, hub, orb = orbs[i], target, violet, i] {
            auto const start = hub->convertToWorldSpace(orb->getPosition());
            auto const local = actors->convertToNodeSpace(start);
            orb->retain();
            orb->removeFromParentAndCleanup(false);
            orb->setPosition(local);
            actors->addChild(orb, 10);
            orb->release();
            ccBezierConfig path;
            path.endPosition = target + CCPoint{0.f, 20.f};
            path.controlPoint_1 = local + CCPoint{0.f, 70.f + i * 18.f};
            path.controlPoint_2 = path.endPosition + CCPoint{0.f, 60.f - i * 40.f};
            orb->runAction(CCSequence::create(CCEaseSineIn::create(CCBezierTo::create(.3f, path)),
                                              CallFuncExt::create([actors, end = path.endPosition, violet] {
                                                  flare(actors, end, violet, 22.f, .25f, 14, 6);
                                                  nativeWave(actors, end, violet, 30.f);
                                                  audio::fx("fx_orbfall", "playSound_01.ogg");
                                              }),
                                              CCRemoveSelf::create(), nullptr));
        });
    }
    float const strike = t0 + 1.32f;
    at(actors, strike - .22f, [actors, target, azure] {
        auto* warning = addGlow(actors, target + CCPoint{0.f, 20.f}, 6, [azure](CCDrawNode* d, float a) {
            d->drawSegment({0, -40}, {0, 220}, 1.f, rgba(azure, a * .7f));
        });
        warning->runAction(CCSequence::create(CCFadeOut::create(.22f), CCRemoveSelf::create(), nullptr));
    });
    at(actors, strike, [actors, target, violet, azure] {
        auto* pillar = addGlow(actors, target + CCPoint{0.f, 20.f}, 12, [violet](CCDrawNode* d, float a) {
            d->drawSegment({0, -45}, {0, 240}, 22.f, rgba(violet, a * .22f));
            d->drawSegment({0, -45}, {0, 240}, 11.f, rgba(violet, a * .6f));
            d->drawSegment({0, -45}, {0, 240}, 4.f, {1, 1, 1, a});
        });
        pillar->setScaleX(.1f);
        pillar->runAction(CCSequence::create(
            CCEaseExponentialOut::create(CCScaleTo::create(.08f, 1.3f, 1.f)), CCDelayTime::create(.12f),
            CCSpawn::create(CCScaleTo::create(.4f, .05f, 1.f), CCFadeOut::create(.4f), nullptr), CCRemoveSelf::create(),
            nullptr));
        nativeWave(actors, target, azure, 110.f);
        nativeBurst(actors, target + CCPoint{0.f, 20.f}, "glitterEffectIcon.plist", ccWHITE, 26, .1f, .5f, 160.f, 7.f);
    });
    return strike;
}

// 사각 입자 파티클 (회전하는 조각 효과).
CCParticleSystemQuad* squares(CCNode* parent, CCPoint at, ccColor3B tint, unsigned count, float duration, float life,
                              float speed, float size) {
    auto* particles = nativeBurst(parent, at, "explodeEffect.plist", tint, count, duration, life, speed, size);
    if (!particles) return nullptr;
    if (auto* texture = CCTextureCache::sharedTextureCache()->addImage("square.png", false))
        particles->setTexture(texture);
    particles->setStartSpin(0.f);
    particles->setStartSpinVar(180.f);
    particles->setEndSpin(270.f);
    particles->setEndSpinVar(180.f);
    return particles;
}

// 불사조 노드 생성. 몸통, 날개 둘(tag 100/101, 뒤쪽 날개는 어둡고 작게), 빛무리로 구성된다.
CCNodeRGBA* firebird(art::PhoenixColors const& c) {
    auto* bird = CCNodeRGBA::create();
    bird->setCascadeOpacityEnabled(true);
    auto* aura = Glow::create([c](CCDrawNode* d, float a) { softDot(d, {-4.f, 4.f}, 34.f, c.primary, a * .75f); });
    bird->addChild(aura, -3);
    art::PhoenixColors const shadow{art::dark(c.primary, .3f), art::dark(c.secondary, .3f), art::dark(c.deep, .3f)};
    auto* backWing = Glow::create([shadow](CCDrawNode* d, float a) { art::phoenixWing(d, shadow, a); }, false);
    backWing->setPosition(art::phoenixShoulder() + CCPoint{3.f, 1.5f});
    backWing->setScale(.85f);
    backWing->setTag(101);
    bird->addChild(backWing, -1);
    bird->addChild(Glow::create([c](CCDrawNode* d, float a) { art::phoenixBody(d, c, a); }, false), 0);
    auto* wing = Glow::create([c](CCDrawNode* d, float a) { art::phoenixWing(d, c, a); }, false);
    wing->setPosition(art::phoenixShoulder());
    wing->setTag(100);
    bird->addChild(wing, 1);
    return bird;
}

// 불사조 날개를 period 간격으로 beats번 펄럭이게 한다.
void flap(CCNode* bird, float period, int beats) {
    for (int tag : {100, 101}) {
        auto* wing = bird->getChildByTag(tag);
        if (!wing) continue;
        float const base = tag == 100 ? 1.f : .85f;
        wing->runAction(CCRepeat::create(
            CCSequence::create(CCEaseSineInOut::create(CCScaleTo::create(period * .45f, base, base * .2f)),
                               CCEaseSineInOut::create(CCScaleTo::create(period * .55f, base, base)), nullptr),
            beats));
    }
}

// 불사조 모양의 잔상 하나를 남기고 사라지게 한다 / 일정 간격으로 잔상을 count번 만든다.
void ghost(CCNode* actors, CCNode* bird, ccColor3B tint) {
    auto* copy = Glow::create([tint](CCDrawNode* d, float a) { art::phoenixSilhouette(d, tint, a * .55f); });
    copy->setPosition(bird->getPosition());
    copy->setScaleX(bird->getScaleX());
    copy->setScaleY(bird->getScaleY());
    copy->setRotation(bird->getRotation());
    actors->addChild(copy, 10);
    copy->runAction(CCSequence::create(CCFadeOut::create(.26f), CCRemoveSelf::create(), nullptr));
}

void trail(CCNode* actors, CCNode* bird, ccColor3B tint, int count) {
    bird->runAction(CCRepeat::create(
        CCSequence::create(CCDelayTime::create(.045f),
                           CallFuncExt::create([actors, bird, tint] { ghost(actors, bird, tint); }), nullptr),
        count));
}

// 땅에서 솟는 불꽃 가시 7개.
void flameSpikes(CCNode* actors, CCPoint at, art::PhoenixColors const& c, float height) {
    for (int i = 0; i < 7; ++i) {
        float const x = (i - 3) * 9.f, h = height * (i % 2 ? .62f : 1.f) * (1.f - std::abs(i - 3) * .12f);
        auto* spike = Glow::create(
            [c, h, i](CCDrawNode* d, float a) {
                art::Path outer{{-7.f, 0.f}, {7.f, 0.f}, {0.f, h}};
                art::stroke(d, outer, 1.1f, {art::kInk.r, art::kInk.g, art::kInk.b, a});
                art::fill(d, outer, art::f4(i % 2 ? c.secondary : c.primary, a));
                art::fill(d, {{-3.5f, 0.f}, {3.5f, 0.f}, {0.f, h * .6f}}, art::f4(art::light(c.secondary, .6f), a));
            },
            false);
        spike->setPosition(at + CCPoint{x, -16.f});
        spike->setScaleY(0.f);
        actors->addChild(spike, 14);
        spike->runAction(
            CCSequence::create(CCDelayTime::create(std::abs(i - 3) * .03f),
                               CCEaseBackOut::create(CCScaleTo::create(.16f, 1.f, 1.f)), CCDelayTime::create(.28f),
                               CCSpawn::create(CCScaleTo::create(.22f, 1.f, 0.f), CCFadeOut::create(.22f), nullptr),
                               CCRemoveSelf::create(), nullptr));
    }
}

// kind 6: 불사조. 하늘이 붉게 물들고 불사조가 날아올라 loser 위로 간 뒤 급강하해 부딪힌다.
// 충돌 시점(impact)을 돌려준다.
float phoenix(CCNode* actors, CCNode* winner, CCNode* loser, float direction, float t0) {
    art::PhoenixColors const colors;
    ccColor3B const ember = colors.primary, gold = colors.secondary;
    auto const window = actors->getContentSize();
    auto const target = loser->getPosition();
    auto const origin = winner->getPosition();
    auto const apex = CCPoint{target.x - direction * 34.f, std::min(window.height - 40.f, target.y + 115.f)};
    auto* sky = CCLayerColor::create(ccc4(255, 80, 20, 0), window.width, window.height);
    actors->addChild(sky, -6);
    sky->runAction(CCSequence::create(CCDelayTime::create(t0 - .2f), CCFadeTo::create(.5f, 60),
                                      CCDelayTime::create(1.5f), CCFadeTo::create(.5f, 0), CCRemoveSelf::create(),
                                      nullptr));
    at(actors, t0 - .2f, [actors, origin, ember, gold] {
        convergeBurst(actors, origin, gold, 36, .35f, 70.f);
        nativeWave(actors, origin, gold, 60.f);
        squares(actors, origin, ember, 24, .25f, .45f, 90.f, 7.f);
        audio::fx("fx_phoenix", "unlockPath.ogg");
    });
    auto* bird = firebird(colors);
    bird->setPosition(origin);
    bird->setScale(.2f);
    bird->setScaleX(.2f * direction);
    bird->setVisible(false);
    actors->addChild(bird, 11);
    ccBezierConfig climb;
    climb.controlPoint_1 = CCPoint{origin.x + direction * 20.f, origin.y + 150.f};
    climb.controlPoint_2 = CCPoint{apex.x - direction * 90.f, apex.y + 40.f};
    climb.endPosition = apex;
    float const size = 1.15f;
    bird->runAction(CCSequence::create(
        CCDelayTime::create(t0), CCShow::create(),
        CCSpawn::create(CCEaseSineOut::create(CCBezierTo::create(.75f, climb)),
                        CCEaseBackOut::create(CCScaleTo::create(.4f, direction * size, size)), nullptr),
        CCEaseSineInOut::create(CCMoveBy::create(.45f, {direction * 34.f, 10.f})),
        CCSpawn::create(CCScaleTo::create(.12f, direction * size * .85f, size * .85f),
                        CCRotateTo::create(.12f, direction * 62.f), nullptr),
        CCEaseIn::create(CCMoveTo::create(.17f, target), 2.f),
        CCSpawn::create(CCEaseSineOut::create(CCMoveBy::create(.4f, {direction * 60.f, -window.height})),
                        CCFadeOut::create(.3f), nullptr),
        CCRemoveSelf::create(), nullptr));
    at(actors, t0, [actors, bird, ember, gold, direction] {
        flap(bird, .24f, 5);
        audio::fx("fx_wing");
        audio::fx("fx_dragon");
        trail(actors, bird, gold, 30);
        if (auto* sparks = squares(bird, {-14.f, 0.f}, ember, 80, 1.6f, .45f, 45.f, 6.f)) {
            sparks->setPositionType(kCCPositionTypeFree);
            sparks->setGravity({0.f, 30.f});
            sparks->setAngle(direction > 0 ? 190.f : -10.f);
            sparks->setAngleVar(35.f);
            sparks->setZOrder(-4);
        }
    });
    float const fold = t0 + 1.2f;
    at(actors, fold, [actors, apex, gold, loser] {
        flare(actors, apex, gold, 40.f, .3f, 12, 6);
        nativeWave(actors, apex, gold, 70.f);
        loser->runAction(CCTintTo::create(.3f, 255, 170, 110));
        shake(loser, .35f, 2.f);
        audio::fx("fx_dive", "playSound_01.ogg");
        audio::fx("fx_firelaser");
    });
    float const impact = fold + .12f + .17f;
    at(actors, impact, [actors, target, ember, gold, colors] {
        flameSpikes(actors, target, colors, 70.f);
        nativeWave(actors, target, gold, 130.f);
        squares(actors, target, ember, 70, .12f, .7f, 210.f, 9.f);
        if (auto* rising = squares(actors, target, gold, 36, .5f, .9f, 60.f, 6.f)) rising->setGravity({0.f, 110.f});
    });
    winner->runAction(CCSequence::create(
        CCDelayTime::create(t0 - .2f), CCEaseSineOut::create(CCMoveBy::create(.3f, {0.f, 6.f})),
        CCDelayTime::create(1.4f), CCEaseSineIn::create(CCMoveBy::create(.3f, {0.f, -6.f})), nullptr));
    return impact;
}

// kind 7: 일식. 어두워진 하늘에서 달이 해를 가리고(totality), 그 순간 winner가 loser를 베어
// 사선으로 화면이 갈라진다.
float eclipse(CCNode* actors, CCNode* winner, CCNode* loser, float direction, float t0) {
    ccColor3B const sunCol = ccc3(255, 214, 120), corona = ccc3(255, 248, 225), steel = ccc3(205, 228, 255);
    auto const window = actors->getContentSize();
    auto const target = loser->getPosition();
    auto const sky = CCPoint{window.width / 2.f, window.height * .72f};
    float const r = std::max(14.f, window.height * .12f);
    float const travel = t0 + .1f, totality = travel + 1.25f, slashAt = totality + .55f;
    auto* dusk = CCLayerColor::create(ccc4(6, 6, 18, 0), window.width, window.height);
    actors->addChild(dusk, -6);
    dusk->runAction(CCSequence::create(CCDelayTime::create(t0 - .2f), CCFadeTo::create(totality - t0 + .2f, 150),
                                       CCDelayTime::create(1.2f), CCFadeOut::create(.4f), CCRemoveSelf::create(),
                                       nullptr));
    auto* sun = addGlow(actors, sky, -3, [r, sunCol, corona](CCDrawNode* d, float a) {
        softDot(d, {}, r * 2.6f, sunCol, a * .8f);
        d->drawDot({}, r * 1.08f, rgba(corona, a * .6f));
        d->drawDot({}, r, rgba(sunCol, a));
        d->drawDot({}, r * .7f, rgba(corona, a));
    });
    sun->setScale(0.f);
    at(actors, t0 - .3f, [] { audio::fx("fx_grand"); });
    sun->runAction(CCSequence::create(
        CCDelayTime::create(t0 - .3f), CCEaseBackOut::create(CCScaleTo::create(.35f, 1.f)),
        CCDelayTime::create(totality - t0 + 1.5f), CCFadeOut::create(.3f), CCRemoveSelf::create(), nullptr));
    auto* moon = Glow::create(
        [r](CCDrawNode* d, float a) {
            d->drawDot({}, r * 1.04f, {.72f, .76f, .86f, a * .8f});
            d->drawDot({}, r * 1.0f, {.02f, .02f, .05f, a});
            d->drawDot({r * .3f, r * .25f}, r * .14f, {.08f, .08f, .13f, a});
            d->drawDot({-r * .35f, -r * .2f}, r * .1f, {.08f, .08f, .13f, a});
        },
        false);
    auto const moonStart = sky + CCPoint{-direction * r * 5.5f, 0.f};
    moon->setPosition(moonStart);
    moon->setScale(0.f);
    actors->addChild(moon, 21);
    moon->runAction(CCSequence::create(
        CCDelayTime::create(t0 - .2f), CCEaseBackOut::create(CCScaleTo::create(.3f, 1.f)),
        CCDelayTime::create(travel - t0), CCEaseSineIn::create(CCMoveTo::create(totality - travel, sky)),
        CCDelayTime::create(1.5f), CCFadeOut::create(.3f), CCRemoveSelf::create(), nullptr));
    float const cut = -direction * 20.f;
    auto* pivot = CCNode::create();
    pivot->setPosition(target);
    pivot->setRotation(cut);
    actors->addChild(pivot, 20);
    float const big = std::max(window.width, window.height) * 3.f;
    std::array<CCLayerColor*, 2> halves{};
    for (int i = 0; i < 2; ++i) {
        halves[i] = CCLayerColor::create(ccc4(0, 0, 0, 0), big, big / 2.f);
        halves[i]->setPosition({-big / 2.f, i ? 0.f : -big / 2.f});
        pivot->addChild(halves[i]);
        halves[i]->runAction(
            CCSequence::create(CCDelayTime::create(totality - .12f), CCFadeTo::create(.14f, 252), nullptr));
    }
    at(actors, totality, [actors, sky, r, corona] {
        auto* ringGlow = addGlow(actors, sky, 22, [r, corona](CCDrawNode* d, float a) {
            softDot(d, {}, r * 2.2f, corona, a * .5f);
            ring(d, r * 1.07f, 2.2f, rgba(corona, a), 48);
            ring(d, r * 1.22f, .9f, rgba(corona, a * .45f), 48);
            for (int i = 0; i < 12; ++i) {
                float const angle = i * kTau / 12.f + .2f;
                d->drawSegment(polar(r * 1.25f, angle), polar(r * (i % 2 ? 1.6f : 1.9f), angle), 1.f,
                               rgba(corona, a * .5f));
            }
        });
        ringGlow->setScale(.9f);
        ringGlow->runAction(CCSequence::create(CCEaseSineOut::create(CCScaleTo::create(.25f, 1.f)),
                                               CCDelayTime::create(1.2f), CCFadeOut::create(.3f),
                                               CCRemoveSelf::create(), nullptr));
        flare(actors, sky + CCPoint{r * .8f, r * .7f}, ccWHITE, r * 1.5f, .5f, 23, 4);
        audio::fx("fx_boom", "unlockPath.ogg");
    });
    at(actors, slashAt - .22f, [actors, winner, steel, direction] {
        flare(actors, winner->getPosition() + CCPoint{direction * 14.f, 6.f}, steel, 18.f, .22f, 24);
        audio::fx("fx_glint");
    });
    winner->runAction(CCSequence::create(
        CCDelayTime::create(slashAt - .06f),
        CCEaseExponentialIn::create(CCMoveTo::create(.06f, target + CCPoint{direction * 60.f, 0.f})),
        CCDelayTime::create(.9f), CCEaseSineInOut::create(CCMoveTo::create(.35f, winner->getPosition())), nullptr));
    at(actors, slashAt, [actors, target, cut, steel, big, halves, pivot, direction] {
        auto* line = addGlow(actors, target, 26, [big, steel](CCDrawNode* d, float a) {
            float const half = big / 3.f;
            d->drawSegment({-half, 0}, {half, 0}, 12.f, rgba(steel, a * .22f));
            d->drawSegment({-half, 0}, {half, 0}, 3.6f, rgba(steel, a * .9f));
            d->drawSegment({-half, 0}, {half, 0}, 1.3f, {1, 1, 1, a});
        });
        line->setRotation(cut);
        line->setScaleX(.02f);
        line->runAction(CCSequence::create(
            CCEaseExponentialOut::create(CCScaleTo::create(.08f, 1.f, 1.f)), CCDelayTime::create(.18f),
            CCSpawn::create(CCScaleTo::create(.3f, 1.f, .05f), CCFadeOut::create(.3f), nullptr), CCRemoveSelf::create(),
            nullptr));
        slash(actors, target, cut + direction * 10.f, steel, 1.3f);
        audio::fx("fx_slowblade", "magicExplosion.ogg");
        for (int i = 0; i < 2; ++i) {
            float const away = (i ? 1.f : -1.f) * 70.f;
            halves[i]->runAction(
                CCSequence::create(CCDelayTime::create(.1f),
                                   CCSpawn::create(CCEaseSineIn::create(CCMoveBy::create(.5f, {0.f, away})),
                                                   CCFadeOut::create(.45f), nullptr),
                                   nullptr));
        }
        pivot->runAction(CCSequence::create(CCDelayTime::create(.8f), CCRemoveSelf::create(), nullptr));
    });
    return slashAt + .1f;
}

// play()에 돌려주는 연출 결과. impact는 충돌 시점(초), 나머지는 공통 후처리를 켤지 여부
// (camera 줌/흔들림, flyAway loser 날려 보내기, flash 섬광, burst 충격 효과).
struct Finish {
    float impact = 0.f;
    bool camera = true, flyAway = true, flash = true, burst = true;
};

// 스프라이트 프레임/파일 로딩 도우미 (모드 리소스 포함).
CCSpriteFrame* sheetFrame(char const* name, char const* sheet) {
    return loadedFrame(name, sheet);
}

CCSprite* frameSprite(char const* name, char const* sheet) {
    return loadedFrameSprite(name, sheet);
}

CCSprite* modSprite(char const* file, float width) {
    auto const resource = std::string(Mod::get()->expandSpriteName(file));
    auto* sprite = fileSprite(resource.c_str());
    if (!sprite) return nullptr;
    sprite->setScale(width / std::max(1.f, sprite->getContentSize().width));
    return sprite;
}

// SimplePlayer 아이콘이면 그 1P 색을, 아니면 fallback을 돌려준다 (파편 색에 사용).
ccColor3B iconColor(CCNode* node, ccColor3B fallback) {
    auto* player = typeinfo_cast<SimplePlayer*>(node);
    return player && player->m_firstLayer ? player->m_firstLayer->getColor() : fallback;
}

// node 하위의 색 지정 가능한 노드를 모두 to 색으로 물들인다. 파티클은 절반 시간에 숨긴다.
void tintTree(CCNode* node, ccColor3B to, float time) {
    auto* children = node ? node->getChildren() : nullptr;
    if (!children) return;
    for (auto* child : CCArrayExt<CCNode*>(children)) {
        if (typeinfo_cast<CCParticleSystem*>(child)) {
            child->runAction(CCSequence::create(CCDelayTime::create(time * .5f), CCHide::create(), nullptr));
            continue;
        }
        if (typeinfo_cast<CCRGBAProtocol*>(child)) child->runAction(CCTintTo::create(time, to.r, to.g, to.b));
        tintTree(child, to, time);
    }
}

// 괴수의 머리/턱 위치와 회전 포즈. closed(닫힘), open(벌림) 두 포즈 사이를 open(0~1)으로 보간한다.
struct BeastPose {
    CCPoint head, jaw;
    float headTurn, jawTurn;
};
constexpr BeastPose kBeastClosed{{.775f, 11.5f}, {.225f, -10.45f}, 0.f, 0.f};
constexpr BeastPose kBeastOpen{{9.625f, 24.175f}, {11.9f, -23.625f}, 40.15f, -37.29f};
constexpr float kBeastMouth = 22.f;

// 괴수(머리와 턱 스프라이트, 각각 본체와 발광 두 겹)와 입 동작 한 번(시각, 벌림, 소요 시간).
struct Beast {
    CCNode* root = nullptr;
    std::array<CCSprite*, 2> head{}, jaw{};
};
struct Chew {
    float at, open, time;
};

BeastPose beastPose(float open) {
    auto mix = [open](CCPoint a, CCPoint b) { return a + (b - a) * open; };
    return {mix(kBeastClosed.head, kBeastOpen.head), mix(kBeastClosed.jaw, kBeastOpen.jaw), kBeastOpen.headTurn * open,
            kBeastOpen.jawTurn * open};
}

// 괴수 스프라이트를 만들어 닫힌 포즈로 둔다. 리소스가 없으면 root가 nullptr.
Beast makeBeast() {
    Beast beast;
    char const* sheet = "FireSheet_01.plist";
    auto* head = frameSprite("GJBeast01_01_001.png", sheet);
    auto* jaw = frameSprite("GJBeast01_02_001.png", sheet);
    if (!head || !jaw) return beast;
    beast.root = CCNode::create();
    beast.head[0] = head;
    beast.jaw[0] = jaw;
    beast.head[1] = frameSprite("GJBeast01_01_glow_001.png", sheet);
    beast.jaw[1] = frameSprite("GJBeast01_02_glow_001.png", sheet);
    for (auto* glow : {beast.head[1], beast.jaw[1]}) {
        if (!glow) continue;
        glow->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        glow->setOpacity(210);
        beast.root->addChild(glow, -1);
    }
    head->setColor(ccc3(12, 10, 18));
    jaw->setColor(ccc3(12, 10, 18));
    beast.root->addChild(jaw, 0);
    beast.root->addChild(head, 1);
    auto const closed = beastPose(0.f);
    for (int i = 0; i < 2; ++i) {
        if (beast.head[i]) beast.head[i]->setPosition(closed.head);
        if (beast.jaw[i]) beast.jaw[i]->setPosition(closed.jaw);
    }
    return beast;
}

// chews 목록대로 머리와 턱을 움직이는 액션을 만든다 (입을 벌렸다 물기를 반복).
void chewTimeline(Beast const& beast, std::vector<Chew> const& chews) {
    auto build = [&chews](bool head) {
        auto* actions = CCArray::create();
        float clock = 0.f;
        for (auto const& chew : chews) {
            if (chew.at > clock) actions->addObject(CCDelayTime::create(chew.at - clock));
            auto const pose = beastPose(chew.open);
            actions->addObject(CCSpawn::create(
                CCEaseSineInOut::create(CCMoveTo::create(chew.time, head ? pose.head : pose.jaw)),
                CCEaseSineInOut::create(CCRotateTo::create(chew.time, head ? pose.headTurn : pose.jawTurn)), nullptr));
            clock = std::max(clock, chew.at) + chew.time;
        }
        return CCSequence::create(actions);
    };
    for (int i = 0; i < 2; ++i) {
        if (beast.head[i]) beast.head[i]->runAction(build(true));
        if (beast.jaw[i]) beast.jaw[i]->runAction(build(false));
    }
}

// kind 8: 괴수가 화면 밖에서 뛰어들어 loser를 한입에 먹고 씹은 뒤 돌아간다.
// 리소스가 없으면 kind 1(가시)로 대체한다.
Finish monster(CCNode* actors, CCNode* winner, CCNode* loser, float direction, float t0) {
    auto beast = makeBeast();
    if (!beast.root) return {spikes(actors, winner, direction, t0)};
    auto const window = actors->getContentSize();
    auto const target = loser->getPosition();
    float const scale = std::max(.4f, loser->getScaleY());
    beast.root->setScaleX(scale * direction);
    beast.root->setScaleY(scale);
    auto const start = CCPoint{direction > 0.f ? window.width + 64.f * scale : -64.f * scale, target.y};
    auto const lurk = CCPoint{target.x + direction * (kBeastMouth + 30.f) * scale, target.y};
    auto const bite = CCPoint{target.x + direction * (kBeastMouth + 2.f) * scale, target.y};
    beast.root->setPosition(start);
    actors->addChild(beast.root, 9);
    float const enter = t0 - .2f, chomp = t0 + .72f, leave = t0 + 1.78f;
    beast.root->runAction(CCSequence::create(
        CCDelayTime::create(std::max(0.f, enter)), CCJumpBy::create(.42f, lurk - start, 12.f * scale, 1),
        CCDelayTime::create(chomp - .1f - (enter + .42f)), CCEaseExponentialIn::create(CCMoveTo::create(.1f, bite)),
        CCDelayTime::create(.78f),
        CCEaseSineOut::create(CCScaleTo::create(.09f, scale * direction * .94f, scale * 1.1f)),
        CCEaseSineIn::create(CCScaleTo::create(.12f, scale * direction, scale)),
        CCDelayTime::create(leave - (chomp + .78f + .21f)), CCEaseSineIn::create(CCMoveTo::create(.42f, start)),
        CCHide::create(), nullptr));
    chewTimeline(beast, {{t0 + .02f, .22f, .12f},
                         {t0 + .16f, 0.f, .1f},
                         {t0 + .32f, 1.f, .28f},
                         {chomp - .07f, 0.f, .07f},
                         {t0 + .86f, .38f, .1f},
                         {t0 + .98f, 0.f, .07f},
                         {t0 + 1.08f, .42f, .1f},
                         {t0 + 1.2f, 0.f, .07f},
                         {t0 + 1.3f, .36f, .1f},
                         {t0 + 1.42f, 0.f, .07f},
                         {leave - .05f, .3f, .15f},
                         {leave + .2f, 0.f, .12f}});
    at(actors, enter - .1f, [loser] {
        shake(loser, .28f, 1.4f);
        audio::fx("fx_stomp", "playSound_01.ogg");
    });
    at(actors, enter + .42f, [actors, lurk, scale] {
        audio::fx("fx_stomp", "playSound_01.ogg");
        squares(actors, lurk - CCPoint{0.f, 30.f * scale}, ccc3(150, 130, 110), 14, .06f, .35f, 70.f, 4.f);
    });
    at(actors, t0 + .32f, [] { audio::fx("fx_lunge", "playSound_01.ogg"); });
    loser->runAction(CCSequence::create(
        CCDelayTime::create(chomp - .03f),
        CCSpawn::create(CCScaleTo::create(.08f, loser->getScaleX() * .2f, loser->getScaleY() * .2f),
                        CCMoveTo::create(.08f, bite - CCPoint{direction * kBeastMouth * scale * .7f, 0.f}), nullptr),
        CCHide::create(), nullptr));
    auto const crumbs = iconColor(loser, ccc3(255, 140, 180));
    auto const mouth = bite - CCPoint{direction * kBeastMouth * scale, 0.f};
    at(actors, chomp, [actors, mouth, crumbs, scale] {
        audio::fx("fx_chomp", "explode_11.ogg");
        audio::fx("fx_crunch");
        nativeWave(actors, mouth, ccWHITE, 44.f * scale);
        if (auto* bits = squares(actors, mouth, crumbs, 22, .06f, .5f, 120.f, 6.f)) bits->setGravity({0.f, -260.f});
    });
    for (float chew : {t0 + .98f, t0 + 1.2f, t0 + 1.42f}) {
        at(actors, chew, [actors, mouth, crumbs, scale] {
            audio::fx("fx_crunch", "playSound_01.ogg");
            if (auto* bits = squares(actors, mouth - CCPoint{0.f, 6.f * scale}, crumbs, 7, .05f, .45f, 55.f, 4.5f)) {
                bits->setGravity({0.f, -300.f});
                bits->setAngle(-90.f);
                bits->setAngleVar(60.f);
            }
        });
    }
    at(actors, chomp + .78f, [] { audio::fx("fx_tap", "playSound_01.ogg"); });
    at(actors, leave, [] { audio::fx("fx_lunge", "playSound_01.ogg"); });
    winner->runAction(CCSequence::create(CCDelayTime::create(chomp + .12f), CCJumpBy::create(.42f, {0.f, 0.f}, 18.f, 1),
                                         CCJumpBy::create(.32f, {0.f, 0.f}, 9.f, 1), nullptr));
    return {chomp, true, false, false, false};
}

// showdown 연출용 도우미: 단색 다각형 / 선인장 / 유리 깨짐 효과 / 폭발 애니메이션.
void groundShape(CCDrawNode* d, float a, std::vector<CCPoint> const& points, ccColor4F color) {
    if (points.size() < 3) return;
    color.a *= a;
    d->drawPolygon(const_cast<CCPoint*>(points.data()), static_cast<unsigned>(points.size()), color, 0.f, color);
}

void saguaro(CCDrawNode* d, CCPoint foot, float h, ccColor4F c) {
    float const w = h * .16f;
    d->drawSegment(foot, foot + CCPoint{0.f, h}, w, c);
    d->drawSegment(foot + CCPoint{0.f, h * .45f}, foot + CCPoint{-h * .28f, h * .45f}, w * .8f, c);
    d->drawSegment(foot + CCPoint{-h * .28f, h * .45f}, foot + CCPoint{-h * .28f, h * .75f}, w * .8f, c);
    d->drawSegment(foot + CCPoint{0.f, h * .6f}, foot + CCPoint{h * .25f, h * .6f}, w * .75f, c);
    d->drawSegment(foot + CCPoint{h * .25f, h * .6f}, foot + CCPoint{h * .25f, h * .85f}, w * .75f, c);
}

void glassBurst(CCNode* actors, CCPoint at, float size) {
    if (auto* glass = modSprite("lock-on-glass.png", size)) {
        float const full = glass->getScale();
        glass->setPosition(at);
        glass->setScale(full * .35f);
        glass->setRotation(static_cast<float>(std::rand() % 40 - 20));
        actors->addChild(glass, 24);
        glass->runAction(CCSequence::create(
            CCEaseExponentialOut::create(CCScaleTo::create(.1f, full * 1.15f)), CCDelayTime::create(.16f),
            CCSpawn::create(CCScaleTo::create(.32f, full * 1.35f), CCFadeOut::create(.32f), nullptr),
            CCRemoveSelf::create(), nullptr));
    }
    for (int i = 0; i < 9; ++i) {
        auto* shard = modSprite("lock-on-glass.png", size * (.16f + (i % 3) * .05f));
        if (!shard) break;
        float const angle = i * kTau / 9.f + .3f;
        shard->setPosition(at);
        shard->setRotation(static_cast<float>(i * 47 % 360));
        actors->addChild(shard, 23);
        shard->runAction(CCSequence::create(
            CCSpawn::create(
                CCJumpBy::create(.62f, {std::cos(angle) * size * .9f, std::sin(angle) * size * .45f - size * .55f},
                                 size * .35f, 1),
                CCRotateBy::create(.62f, (i % 2 ? 1.f : -1.f) * 420.f),
                CCSequence::create(CCDelayTime::create(.36f), CCFadeOut::create(.26f), nullptr), nullptr),
            CCRemoveSelf::create(), nullptr));
    }
}

// playerExplosion 프레임 12장을 순서대로 재생한다. 프레임을 못 구하면 파티클로 대체.
void deathExplosion(CCNode* actors, CCPoint at, float size) {
    std::vector<CCSpriteFrame*> frames;
    for (int i = 1; i <= 12; ++i) {
        auto const name = fmt::format("playerExplosion_01_{:03}.png", i);
        if (auto* frame = sheetFrame(name.c_str(), "PlayerExplosion_01.plist")) frames.push_back(frame);
    }
    if (frames.size() < 2) {
        squares(actors, at, ccc3(255, 170, 70), 50, .08f, .6f, 200.f, 9.f);
        return;
    }
    auto* blast = CCSprite::createWithSpriteFrame(frames[0]);
    blast->setPosition(at);
    blast->setScale(size / std::max(1.f, blast->getContentSize().width));
    actors->addChild(blast, 22);
    auto* steps = CCArray::create();
    for (size_t i = 1; i < frames.size(); ++i) {
        steps->addObject(CCDelayTime::create(.045f));
        steps->addObject(CallFuncExt::create(
            [blast, frame = Ref<CCSpriteFrame>(frames[i])] { blast->setDisplayFrame(frame.data()); }));
    }
    steps->addObject(CCDelayTime::create(.05f));
    steps->addObject(CCRemoveSelf::create());
    blast->runAction(CCSequence::create(steps));
}

// kind 9: 서부 결투. winner가 모자를 쓰고 리볼버를 돌린 뒤 컷(cut)으로 석양 배경 장면으로 전환되어
// 서로 엇갈려 지나가고, 총을 쏘면 loser가 유리처럼 깨지며 폭발한다.
// 이 연출은 자체 장면을 쓰므로 공통 후처리를 모두 끈다. 모자/총 리소스가 없으면 kind 3(레일건)으로 대체한다.
Finish showdown(CCNode* actors, CCNode* winner, CCNode* loser, float direction, float t0) {
    auto* hat = modSprite("lock-on-hat.png", 40.f);
    auto* gun = modSprite("lock-on-revolver.png", 30.f);
    if (!hat || !gun) return {railgun(actors, winner, loser, direction, t0)};
    auto const stage = actors->getContentSize();
    float const W = stage.width, H = stage.height;
    float const base = winner->getScaleY(), loserBase = loser->getScaleY();
    float const horizon = H * .4f, front = H * .27f, back = horizon + 15.f * loserBase * .55f - 2.f;
    float const cut = t0 + .9f, cross = cut + .05f, crossTime = 1.3f;
    float const shadow = cross + crossTime + .02f, glint = shadow + .3f, fire = glint + .24f;
    float const hit = fire + .07f, boom = hit + .06f;
    auto* icon = typeinfo_cast<SimplePlayer*>(winner);
    auto const centre =
        icon && icon->m_firstLayer ? icon->m_firstLayer->getPosition() : winner->getAnchorPointInPoints();
    auto place = [hat, gun, centre](float facing) {
        hat->setFlipX(facing < 0.f);
        gun->setFlipX(facing < 0.f);
        gun->setPosition(centre + CCPoint{facing * 23.f, -5.f});
        gun->setRotation(0.f);
        return centre + CCPoint{facing * -2.f, 22.5f};
    };
    auto const hatSeat = place(direction);
    float const hatScale = hat->getScale(), gunScale = gun->getScale();
    hat->setPosition(hatSeat + CCPoint{direction * 6.f, 64.f});
    hat->setRotation(-38.f * direction);
    hat->setOpacity(0);
    winner->addChild(hat, 5);
    gun->setPosition(centre + CCPoint{direction * -8.f, -30.f});
    gun->setScale(0.f);
    winner->addChild(gun, 6);
    hat->runAction(CCSequence::create(
        CCDelayTime::create(t0),
        CCSpawn::create(CCFadeIn::create(.08f), CCEaseIn::create(CCMoveTo::create(.3f, hatSeat), 2.2f),
                        CCRotateTo::create(.3f, -9.f * direction), nullptr),
        CCScaleTo::create(.06f, hatScale * 1.16f, hatScale * .82f),
        CCEaseBackOut::create(CCScaleTo::create(.16f, hatScale)), CCDelayTime::create(.04f),
        CCEaseSineInOut::create(CCRotateBy::create(.13f, 10.f * direction)),
        CCEaseSineInOut::create(CCRotateBy::create(.13f, -10.f * direction)), nullptr));
    winner->runAction(CCSequence::create(CCDelayTime::create(t0 + .3f),
                                         CCScaleTo::create(.06f, base * 1.08f, base * .86f),
                                         CCEaseBackOut::create(CCScaleTo::create(.16f, base)), nullptr));
    at(actors, t0, [] { audio::fx("fx_hat", "playSound_01.ogg"); });
    at(actors, t0 + .3f, [actors, winner] {
        audio::fx("fx_tap");
        squares(actors, winner->getPosition() + CCPoint{0.f, 20.f}, ccc3(170, 120, 70), 8, .05f, .3f, 50.f, 3.5f);
    });
    gun->runAction(CCSequence::create(
        CCDelayTime::create(t0 + .34f),
        CCSpawn::create(CCEaseSineOut::create(CCMoveTo::create(.36f, centre + CCPoint{direction * 23.f, -5.f})),
                        CCEaseSineOut::create(CCRotateBy::create(.36f, direction * 720.f)),
                        CCEaseBackOut::create(CCScaleTo::create(.36f, gunScale)), nullptr),
        nullptr));
    at(actors, t0 + .34f, [] { audio::fx("fx_spin", "playSound_01.ogg"); });
    at(actors, t0 + .72f, [actors, winner, direction, centre] {
        audio::fx("fx_reload", "unlockPath.ogg");
        auto const spot =
            actors->convertToNodeSpace(winner->convertToWorldSpace(centre + CCPoint{direction * 30.f, -2.f}));
        flare(actors, spot, ccc3(255, 230, 160), 16.f, .28f, 14);
    });
    auto* scene = CCNodeRGBA::create();
    scene->setCascadeOpacityEnabled(true);
    scene->setVisible(false);
    actors->addChild(scene, -9);
    auto gradient = [W, H](ccColor4B top, ccColor4B bottom) {
        auto* layer = CCLayerGradient::create(top, bottom);
        layer->setContentSize({W * 3.f, H * 1.2f});
        layer->setPosition({-W, -H * .1f});
        return layer;
    };
    auto* day = gradient(ccc4(255, 112, 44, 255), ccc4(255, 214, 120, 255));
    auto* dusk = gradient(ccc4(54, 18, 74, 255), ccc4(255, 84, 44, 255));
    scene->addChild(day, 0);
    dusk->setOpacity(0);
    scene->addChild(dusk, 1);
    float const sunRadius = std::max(10.f, H * .12f);
    auto* sun = Glow::create([sunRadius](CCDrawNode* d, float a) {
        softDot(d, {}, sunRadius * 2.8f, ccc3(255, 170, 70), a * .9f);
        d->drawDot({}, sunRadius * 1.06f, {1.f, .62f, .26f, a * .7f});
        d->drawDot({}, sunRadius, {1.f, .78f, .38f, a});
        d->drawDot({}, sunRadius * .78f, {1.f, .93f, .66f, a});
    });
    sun->setPosition({W * .6f, H * .7f});
    scene->addChild(sun, 2);
    auto* rays = Glow::create([sunRadius](CCDrawNode* d, float a) {
        for (int i = 0; i < 9; ++i) {
            float const angle = i * kTau / 9.f;
            d->drawSegment(polar(sunRadius * 1.4f, angle), polar(sunRadius * 5.f, angle), sunRadius * .22f,
                           {1.f, .8f, .45f, a * .16f});
        }
    });
    rays->setPosition(sun->getPosition());
    scene->addChild(rays, 2);
    auto* mesas = Glow::create(
        [W, horizon](CCDrawNode* d, float a) {
            ccColor4F const rock{.33f, .12f, .2f, 1.f};
            groundShape(d, a,
                        {{-W * .2f, horizon},
                         {-W * .1f, horizon + 26.f},
                         {W * .12f, horizon + 26.f},
                         {W * .18f, horizon + 12.f},
                         {W * .3f, horizon}},
                        rock);
            groundShape(d, a,
                        {{W * .62f, horizon},
                         {W * .7f, horizon + 34.f},
                         {W * .86f, horizon + 34.f},
                         {W * .9f, horizon + 18.f},
                         {W * 1.05f, horizon + 16.f},
                         {W * 1.1f, horizon}},
                        rock);
            groundShape(
                d, a,
                {{W * 1.25f, horizon}, {W * 1.3f, horizon + 20.f}, {W * 1.42f, horizon + 20.f}, {W * 1.48f, horizon}},
                rock);
        },
        false);
    scene->addChild(mesas, 3);
    auto* ground = Glow::create(
        [W, H, horizon](CCDrawNode* d, float a) {
            groundShape(d, a, {{-W, -H}, {W * 2.f, -H}, {W * 2.f, horizon}, {-W, horizon}}, {.2f, .07f, .1f, 1.f});
            groundShape(d, a, {{-W, horizon - 7.f}, {W * 2.f, horizon - 7.f}, {W * 2.f, horizon}, {-W, horizon}},
                        {.42f, .17f, .16f, 1.f});
            for (int i = 0; i < 7; ++i) {
                float const y = horizon - 16.f - i * 13.f;
                d->drawSegment({-W, y}, {W * 2.f, y}, .6f, {.48f, .2f, .16f, a * (.35f - i * .04f)});
            }
        },
        false);
    scene->addChild(ground, 4);
    auto* cacti = Glow::create(
        [W, H, front](CCDrawNode* d, float a) {
            ccColor4F const ink{.08f, .03f, .06f, a};
            saguaro(d, {W * .08f, front - 18.f}, H * .3f, ink);
            saguaro(d, {W * .55f, front - 26.f}, H * .22f, ink);
            saguaro(d, {W * 1.05f, front - 20.f}, H * .34f, ink);
            saguaro(d, {W * 1.45f, front - 22.f}, H * .26f, ink);
        },
        false);
    cacti->setVisible(false);
    actors->addChild(cacti, 30);
    auto* weed = Glow::create(
        [](CCDrawNode* d, float a) {
            ccColor4F const brush{.36f, .2f, .12f, a};
            for (int i = 0; i < 9; ++i) {
                float const r = 4.f + i * .9f, s = i * .7f;
                for (int k = 0; k < 6; ++k)
                    d->drawSegment(polar(r, s + k * 1.05f), polar(r * .7f, s + k * 1.05f + .6f), .5f, brush);
            }
        },
        false);
    weed->setPosition({-20.f, horizon + 4.f});
    weed->setScale(std::max(.5f, H / 200.f));
    scene->addChild(weed, 5);
    auto* flash = CCLayerColor::create(ccc4(255, 196, 120, 0), W * 3.f, H * 3.f);
    flash->setPosition({-W, -H});
    actors->addChild(flash, 40);
    flash->runAction(CCSequence::create(CCDelayTime::create(cut - .08f), CCFadeTo::create(.08f, 255),
                                        CCDelayTime::create(.05f), CCFadeTo::create(.32f, 0), CCRemoveSelf::create(),
                                        nullptr));
    scene->runAction(CCSequence::create(CCDelayTime::create(cut), CCShow::create(), nullptr));
    cacti->runAction(CCSequence::create(CCDelayTime::create(cut), CCShow::create(),
                                        CCEaseSineInOut::create(CCMoveBy::create(crossTime + .1f, {-W * .42f, 0.f})),
                                        nullptr));
    mesas->runAction(CCSequence::create(CCDelayTime::create(cut),
                                        CCEaseSineInOut::create(CCMoveBy::create(crossTime + .1f, {-W * .07f, 0.f})),
                                        nullptr));
    weed->runAction(
        CCSequence::create(CCDelayTime::create(cut),
                           CCSpawn::create(CCRotateBy::create(crossTime + 1.8f, 900.f),
                                           CCJumpBy::create(crossTime + 1.8f, {W * 1.3f, 0.f}, 6.f, 5), nullptr),
                           nullptr));
    sun->runAction(CCSequence::create(
        CCDelayTime::create(cut),
        CCEaseSineIn::create(CCMoveTo::create(boom - cut, {W * .6f, horizon + sunRadius * .15f})), nullptr));
    rays->runAction(CCSequence::create(
        CCDelayTime::create(cut),
        CCSpawn::create(CCEaseSineIn::create(CCMoveTo::create(boom - cut, {W * .6f, horizon + sunRadius * .15f})),
                        CCRotateBy::create(boom - cut, 24.f), nullptr),
        nullptr));
    dusk->runAction(CCSequence::create(CCDelayTime::create(cut + .2f), CCFadeTo::create(boom - cut, 255), nullptr));
    at(actors, cut, [actors, winner, loser, hat, place, W, front, back, base, loserBase] {
        if (auto* sprite = typeinfo_cast<CCSprite*>(winner)) sprite->setFlipX(false);
        winner->setRotation(0.f);
        winner->setScale(base * 1.45f);
        winner->setPosition({W * .86f, front});
        hat->setPosition(place(1.f));
        hat->setRotation(-9.f);
        actors->reorderChild(winner, 12);
        if (auto* sprite = typeinfo_cast<CCSprite*>(loser)) sprite->setFlipX(true);
        loser->setRotation(0.f);
        loser->setScale(loserBase * .55f);
        loser->setPosition({W * .14f, back});
        actors->reorderChild(loser, 6);
        audio::fx("fx_hoon", "playSound_01.ogg");
    });
    winner->runAction(CCSequence::create(CCDelayTime::create(cross),
                                         CCJumpBy::create(crossTime, {-W * .56f, 0.f}, 4.f * base, 5), nullptr));
    loser->runAction(CCSequence::create(CCDelayTime::create(cross),
                                        CCJumpBy::create(crossTime, {W * .56f, 0.f}, 1.6f * loserBase, 5), nullptr));
    at(actors, shadow, [winner] {
        tintTree(winner, ccc3(4, 2, 8), .32f);
        if (auto* aura = winner->getChildByID("membership-aura-image"_spr)) aura->runAction(CCFadeOut::create(.2f));
    });
    at(actors, glint, [actors, winner, centre] {
        auto const face = actors->convertToNodeSpace(winner->convertToWorldSpace(centre + CCPoint{6.f, 5.f}));
        flare(actors, face, ccWHITE, 9.f * winner->getScaleY(), .34f, 26, 4);
        audio::fx("fx_glint");
    });
    at(actors, fire, [actors, winner, loser, gun, base] {
        auto const muzzle = actors->convertToNodeSpace(
            gun->convertToWorldSpace({gun->getContentSize().width * .98f, gun->getContentSize().height * .78f}));
        auto const target = loser->getPosition();
        audio::fx("fx_revolver", "explode_11.ogg");
        flare(actors, muzzle, ccc3(255, 226, 140), 22.f * base, .22f, 27, 8);
        nativeWave(actors, muzzle, ccc3(255, 214, 140), 30.f * base);
        if (auto* fire =
                nativeBurst(actors, muzzle, "fireballEffect.plist", ccc3(255, 170, 60), 18, .05f, .18f, 120.f, 6.f)) {
            fire->setAngle(0.f);
            fire->setAngleVar(25.f);
            fire->setZOrder(26);
        }
        beam(actors, muzzle, target, ccc3(255, 236, 170), 3.5f, .14f, 25);
        gun->runAction(CCSequence::create(CCRotateBy::create(.05f, -28.f),
                                          CCEaseSineOut::create(CCRotateBy::create(.3f, 28.f)), nullptr));
        winner->runAction(CCSequence::create(CCMoveBy::create(.05f, {-6.f * base, 0.f}),
                                             CCEaseSineOut::create(CCMoveBy::create(.3f, {6.f * base, 0.f})), nullptr));
        shake(actors, .22f, 3.5f);
    });
    at(actors, hit, [actors, loser, loserBase] {
        glassBurst(actors, loser->getPosition(), 30.f * loserBase * 2.6f);
        audio::fx("fx_glass", "playSound_01.ogg");
    });
    auto const crumbs = iconColor(loser, ccc3(255, 140, 180));
    at(actors, boom, [actors, loser, loserBase, crumbs, W, H] {
        auto const spot = loser->getPosition();
        loser->setVisible(false);
        deathExplosion(actors, spot, 30.f * loserBase * 3.f);
        squares(actors, spot, crumbs, 26, .06f, .55f, 150.f, 5.f);
        nativeWave(actors, spot, ccc3(255, 190, 110), 60.f * loserBase);
        audio::fx("fx_kaboom", "magicExplosion.ogg");
        audio::playBuiltin("explode_11.ogg");
        auto* glow = CCLayerColor::create(ccc4(255, 200, 140, 0), W * 3.f, H * 3.f);
        glow->setPosition({-W, -H});
        actors->addChild(glow, 39);
        glow->runAction(
            CCSequence::create(CCFadeTo::create(.04f, 150), CCFadeTo::create(.3f, 0), CCRemoveSelf::create(), nullptr));
    });
    at(actors, boom + .28f, [actors, gun] {
        audio::fx("fx_shell");
        auto const box = gun->getContentSize();
        auto const from = actors->convertToNodeSpace(gun->convertToWorldSpace({box.width * .5f, box.height * .5f}));
        auto* shell = addGlow(actors, from, 14, [](CCDrawNode* d, float a) {
            d->drawSegment({-2.5f, 0}, {2.5f, 0}, 1.4f, {1.f, .78f, .3f, a});
        });
        shell->runAction(CCSequence::create(
            CCSpawn::create(CCJumpBy::create(.5f, {-16.f, -24.f}, 16.f, 1), CCRotateBy::create(.5f, -540.f), nullptr),
            CCFadeOut::create(.15f), CCRemoveSelf::create(), nullptr));
    });
    return {hit, false, false, false, false};
}
}

// kind별 연출 전체 길이(초). 9번만 길다.
float length(int kind) {
    return kind == 9 ? 3.55f : 3.25f;
}

// 처형 연출 진입점. winner가 loser의 왼쪽이면 direction이 +1(오른쪽으로 진행)이다.
// kind별 연출을 만들고, 돌려받은 충돌 시점에 맞춰 공통 후처리를 예약한다.
void play(CCNode* actors, CCNode* winner, CCNode* loser, int kind, float executionStart) {
    auto const window = actors->getContentSize();
    float const direction = winner->getPositionX() < loser->getPositionX() ? 1.f : -1.f;
    float const t0 = executionStart;
    Finish finish;
    switch (kind) {
    case 2: finish.impact = fireball(actors, winner, direction, t0); break;
    case 3: finish.impact = railgun(actors, winner, loser, direction, t0); break;
    case 4: finish.impact = blade(actors, winner, loser, direction, t0); break;
    case 5: finish.impact = arcane(actors, winner, loser, direction, t0); break;
    case 6: finish.impact = phoenix(actors, winner, loser, direction, t0); break;
    case 7: finish.impact = eclipse(actors, winner, loser, direction, t0); break;
    case 8: finish = monster(actors, winner, loser, direction, t0); break;
    case 9: finish = showdown(actors, winner, loser, direction, t0); break;
    default:
        finish.impact = spikes(actors, winner, direction, t0);
        kind = 1;
        break;
    }
    float const impactDelay = finish.impact;

    // 충돌 후 loser가 회전하며 작아져 날아가 사라진다.
    if (finish.flyAway)
        loser->runAction(CCSequence::create(
            CCDelayTime::create(impactDelay),
            CCSpawn::create(CCEaseSineIn::create(CCRotateBy::create(1.05f, direction * 540.f)),
                            CCEaseSineIn::create(CCMoveBy::create(1.05f, {direction * 135.f, -85.f})),
                            CCEaseSineIn::create(CCScaleTo::create(1.05f, 0.f)), nullptr),
            CCHide::create(), nullptr));
    // 충돌 시점의 효과음 (kind별로 다름. 8, 9번은 자체 연출에서 소리를 낸다).
    at(actors, impactDelay, [kind] {
        switch (kind) {
        case 2:
            audio::fx("fx_burst", "magicExplosion.ogg");
            audio::play(audio::Cue::Blast);
            break;
        case 3:
            audio::fx("fx_kaboom", "magicExplosion.ogg");
            audio::playBuiltin("explode_11.ogg");
            break;
        case 4:
            audio::fx("fx_impale", "magicExplosion.ogg");
            audio::playBuiltin("explode_11.ogg");
            break;
        case 5:
            audio::fx("fx_thunder", "magicExplosion.ogg");
            audio::playBuiltin("magicExplosion.ogg");
            break;
        case 6:
            audio::fx("fx_kaboom", "magicExplosion.ogg");
            audio::fx("fx_skillfire");
            break;
        case 7:
            audio::fx("fx_impale", "magicExplosion.ogg");
            audio::fx("fx_bigboom");
            break;
        case 8:
        case 9: break;
        default:
            audio::fx("fx_thud", "magicExplosion.ogg");
            audio::playBuiltin("explode_11.ogg");
            break;
        }
    });

    // 카메라 연출: 충돌 직전에 줌인하며 기울이고, 충돌 때 순간 확대와 흔들림, 이후 원래 시점으로 복귀.
    if (finish.camera)
        actors->runAction(CCSequence::create(
            CCDelayTime::create(t0 - .25f),
            CCSpawn::create(
                CCEaseSineOut::create(CCScaleTo::create(.25f, 1.12f)),
                CCEaseSineOut::create(CCRotateTo::create(.25f, direction * 2.2f)),
                CCEaseSineOut::create(CCMoveTo::create(.25f, window / 2.f + CCPoint{-direction * 11.f, 4.f})), nullptr),
            CCDelayTime::create(std::max(0.f, impactDelay - t0)),
            CCSpawn::create(CCScaleTo::create(.07f, 1.30f), CCRotateTo::create(.07f, -direction * 3.2f),
                            CCMoveBy::create(.07f, {direction * 14.f, -5.f}), nullptr),
            CCMoveBy::create(.045f, {-direction * 9.f, 5.f}), CCMoveBy::create(.045f, {direction * 7.f, -4.f}),
            CCSpawn::create(CCEaseSineOut::create(CCScaleTo::create(.58f, 1.06f)),
                            CCEaseSineOut::create(CCRotateTo::create(.58f, 0.f)),
                            CCEaseSineOut::create(CCMoveTo::create(.58f, window / 2.f)), nullptr),
            nullptr));

    // 충돌 순간 하얀 섬광.
    if (finish.flash) {
        auto* flash = CCLayerColor::create(ccc4(255, 255, 255, 0));
        flash->setContentSize(window);
        actors->addChild(flash, 28);
        flash->runAction(CCSequence::create(CCDelayTime::create(impactDelay), CCFadeTo::create(.045f, 195),
                                            CCFadeTo::create(.20f, 0), CCRemoveSelf::create(), nullptr));
    }
    // 충돌 효과의 위치와 색 (kind별 색).
    auto const impactPosition = loser->getPosition() + CCPoint{0.f, kind == 5 ? 20.f : 0.f};
    auto const impactTint = kind == 2   ? ccc3(255, 177, 70)
                            : kind == 3 ? ccc3(100, 255, 230)
                            : kind == 4 ? ccc3(255, 120, 195)
                            : kind == 5 ? ccc3(188, 130, 255)
                            : kind == 6 ? ccc3(255, 150, 50)
                            : kind == 7 ? ccc3(210, 230, 255)
                                        : ccc3(125, 220, 255);
    if (finish.burst)
        at(actors, impactDelay,
           [actors, impactPosition, impactTint] { impactEffect(actors, impactPosition, impactTint); });
}
}
