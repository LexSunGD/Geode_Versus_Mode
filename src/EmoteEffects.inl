// 이모트 연출(파티클 같은 스프라이트 효과). 다른 .cpp 안에 #include 되는 파일이다.
// fx 네임스페이스에 재사용 가능한 효과 부품(burst, rise, fall, fountain, orbit, ring,
// glow, rays, slash, twinkles, words, gdBurst)이 있고, emoteEffect()가 이모트 id별로
// 이 부품들을 조합한다. 효과 노드는 스스로 페이드아웃 후 제거된다.
namespace fx {
// 효과용 난수 도우미: 난수 엔진 / 실수 범위 / 0~count-1 정수 / 0xRRGGBB -> ccColor3B
inline std::mt19937& rng() {
    static std::mt19937 engine{std::random_device{}()};
    return engine;
}
inline float rnd(float low, float high) {
    return std::uniform_real_distribution<float>(low, high)(rng());
}
inline int pick(int count) {
    return std::uniform_int_distribution<int>(0, std::max(0, count - 1))(rng());
}
inline ccColor3B rgb(unsigned value) {
    return ccc3((value >> 16) & 255, (value >> 8) & 255, value & 255);
}

using Tints = std::vector<ccColor3B>;

// fx-<name>.png 스프라이트를 size 크기로 만들어 tint를 입힌다. additive면 가산 혼합.
// 리소스가 없으면 nullptr.
inline CCSprite* sprite(char const* name, float size, ccColor3B tint, bool additive = true) {
    auto* result = art::artSprite(std::string("fx-") + name + ".png", size);
    if (!result) return nullptr;
    result->setColor(tint);
    if (additive) result->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
    return result;
}

// node를 from에서 시작해 motion으로 움직이게 하는 공통 실행기. delay 뒤에 나타나서 life 동안
// motion + (뒤쪽 45% 동안 페이드아웃) + endScale로 축소 + 회전(spin)을 동시에 하고, 끝나면 제거한다.
template <class Node>
inline void launch(CCNode* layer, Node* node, CCPoint from, CCFiniteTimeAction* motion, float life, float delay,
                   float endScale = .45f, float spin = 0.f, int z = 20) {
    node->setPosition(from);
    float const base = node->getScale();
    node->setOpacity(0);
    layer->addChild(node, z);
    auto* body = CCSpawn::create(
        motion, CCSequence::create(CCDelayTime::create(life * .55f), CCFadeOut::create(life * .45f), nullptr),
        CCEaseSineIn::create(CCScaleTo::create(life, base * endScale)),
        spin != 0.f ? static_cast<CCFiniteTimeAction*>(CCRotateBy::create(life, spin))
                    : static_cast<CCFiniteTimeAction*>(CCDelayTime::create(0.f)),
        nullptr);
    node->runAction(CCSequence::create(CCDelayTime::create(delay), CCFadeTo::create(.06f, 255), body,
                                       CCRemoveSelf::create(), nullptr));
}

// at에서 사방(또는 spread 각도 범위)으로 퍼지는 파편. inner~outer 거리만큼 감속하며 날아간다.
inline void burst(CCNode* layer, CCPoint at, char const* name, Tints const& tints, int count, float size, float inner,
                  float outer, float life = .75f, float spin = 0.f, float delay = 0.f, float angle = 90.f,
                  float spread = 360.f, bool additive = true) {
    for (int i = 0; i < count; ++i) {
        auto* item = sprite(name, size * rnd(.75f, 1.15f), tints[pick(int(tints.size()))], additive);
        if (!item) return;
        float const a =
            (angle + (spread >= 360.f ? i * 360.f / count + rnd(-12.f, 12.f) : rnd(-spread / 2, spread / 2))) *
            3.14159265f / 180.f;
        float const d = rnd(inner, outer);
        item->setRotation(rnd(-25.f, 25.f));
        launch(layer, item, at,
               CCEaseExponentialOut::create(CCMoveBy::create(life, {std::cos(a) * d, std::sin(a) * d})), life,
               delay + rnd(0.f, .05f), .35f, spin * rnd(-1.f, 1.f));
    }
}

// at에서 위로 곡선을 그리며 떠오르는 스프라이트 (좌우로 흔들림, stagger 안에서 시작 시각 분산).
inline void rise(CCNode* layer, CCPoint at, char const* name, Tints const& tints, int count, float size, float height,
                 float spreadX, float life = 1.1f, float stagger = .5f, bool additive = true) {
    for (int i = 0; i < count; ++i) {
        auto* item = sprite(name, size * rnd(.7f, 1.15f), tints[pick(int(tints.size()))], additive);
        if (!item) return;
        float const dx = rnd(-spreadX, spreadX), sway = rnd(6.f, 14.f) * (rnd(0, 1) < .5f ? -1.f : 1.f);
        float const h = height * rnd(.75f, 1.15f);
        ccBezierConfig path{{dx * .6f, h}, {dx + sway, h * .33f}, {dx - sway, h * .7f}};
        item->setRotation(rnd(-15.f, 15.f));
        launch(layer, item, at + CCPoint{dx, rnd(-6.f, 2.f)}, CCEaseSineOut::create(CCBezierBy::create(life, path)),
               life, rnd(0.f, stagger), .5f, rnd(-30.f, 30.f));
    }
}

// at 위쪽(top)에서 시작해 아래로 drop만큼 떨어지는 스프라이트 (비, 꽃잎, 색종이 등).
inline void fall(CCNode* layer, CCPoint at, char const* name, Tints const& tints, int count, float size, float top,
                 float spreadX, float drop, float life = 1.1f, float spin = 0.f, bool additive = false) {
    for (int i = 0; i < count; ++i) {
        auto* item = sprite(name, size * rnd(.75f, 1.2f), tints[pick(int(tints.size()))], additive);
        if (!item) return;
        float const dx = rnd(-spreadX, spreadX);
        item->setRotation(rnd(0.f, 360.f) * (spin != 0.f));
        launch(layer, item, at + CCPoint{dx, top + rnd(-4.f, 10.f)},
               CCEaseSineIn::create(CCMoveBy::create(life, {rnd(-10.f, 10.f), -drop * rnd(.8f, 1.15f)})), life,
               rnd(0.f, .45f), .7f, spin * rnd(.5f, 1.f) * (rnd(0, 1) < .5f ? -1.f : 1.f));
    }
}

// at에서 위로 솟구쳤다가 포물선으로 떨어지는 스프라이트. side가 0이 아니면 한쪽(+1 오른쪽, -1 왼쪽)으로만 튄다.
inline void fountain(CCNode* layer, CCPoint at, char const* name, Tints const& tints, int count, float size,
                     float height, float spreadX, float life = 1.f, float side = 0.f, bool additive = false) {
    for (int i = 0; i < count; ++i) {
        auto* item = sprite(name, size * rnd(.8f, 1.15f), tints[pick(int(tints.size()))], additive);
        if (!item) return;
        float const dx = side != 0.f ? side * rnd(.5f, 1.f) * spreadX : rnd(-spreadX, spreadX);
        float const h = height * rnd(.7f, 1.1f);
        ccBezierConfig path{{dx, -h * .45f}, {dx * .25f, h}, {dx * .75f, h}};
        launch(layer, item, at + CCPoint{0.f, 4.f}, CCBezierBy::create(life, path), life, rnd(0.f, .25f), .8f,
               rnd(-260.f, 260.f));
    }
}

// at 주변을 타원 궤도로 도는 스프라이트 고리. holder의 Y 스케일로 눌러 타원처럼 보이게 하고,
// 각 스프라이트는 반대로 보정해 모양을 유지한다.
inline void orbit(CCNode* layer, CCPoint at, char const* name, Tints const& tints, int count, float size, float radius,
                  float life = 1.3f, float turns = 1.2f, float squash = .42f, float lift = 0.f) {
    auto* holder = CCNode::create();
    holder->setPosition(at + CCPoint{0.f, lift});
    layer->addChild(holder, 21);
    auto* spinner = CCNode::create();
    holder->addChild(spinner);
    holder->setScaleY(squash);
    for (int i = 0; i < count; ++i) {
        auto* item = sprite(name, size, tints[i % tints.size()]);
        if (!item) break;
        float const a = i * 6.2831853f / count;
        item->setPosition({std::cos(a) * radius, std::sin(a) * radius});
        item->setScaleY(item->getScaleY() / squash);
        item->setOpacity(0);
        item->runAction(CCSequence::create(CCFadeTo::create(.12f, 255), CCDelayTime::create(life - .42f),
                                           CCFadeOut::create(.3f), nullptr));
        spinner->addChild(item);
    }
    spinner->runAction(CCRotateBy::create(life, -360.f * turns));
    holder->runAction(CCSequence::create(CCDelayTime::create(life), CCRemoveSelf::create(), nullptr));
}

// 퍼져 나가는 원형 충격파.
inline void ring(CCNode* layer, CCPoint at, ccColor3B tint, float size, float from = .25f, float to = 1.25f,
                 float life = .5f, float delay = 0.f) {
    auto* item = sprite("ring", size, tint);
    if (!item) return;
    float const base = item->getScale();
    item->setScale(base * from);
    item->setPosition(at);
    item->setOpacity(0);
    layer->addChild(item, 19);
    item->runAction(CCSequence::create(
        CCDelayTime::create(delay), CCFadeTo::create(.03f, 235),
        CCSpawn::create(CCEaseExponentialOut::create(CCScaleTo::create(life, base * to)),
                        CCSequence::create(CCDelayTime::create(life * .35f), CCFadeOut::create(life * .65f), nullptr),
                        nullptr),
        CCRemoveSelf::create(), nullptr));
}

// 잠깐 커졌다가 사라지는 은은한 빛무리.
inline void glow(CCNode* layer, CCPoint at, ccColor3B tint, float size, float life = .55f, float delay = 0.f,
                 GLubyte peak = 210) {
    auto* item = sprite("glow", size, tint);
    if (!item) return;
    float const base = item->getScale();
    item->setScale(base * .5f);
    item->setPosition(at);
    item->setOpacity(0);
    layer->addChild(item, 18);
    item->runAction(
        CCSequence::create(CCDelayTime::create(delay),
                           CCSpawn::create(CCFadeTo::create(.1f, peak),
                                           CCEaseBackOut::create(CCScaleTo::create(.25f, base * 1.15f)), nullptr),
                           CCSpawn::create(CCFadeOut::create(life), CCScaleTo::create(life, base * 1.4f), nullptr),
                           CCRemoveSelf::create(), nullptr));
}

// 중심에서 방사형으로 뻗는 빛줄기. 전체가 천천히 회전한다.
inline void rays(CCNode* layer, CCPoint at, Tints const& tints, int count, float length, float life = 1.2f,
                 float delay = 0.f) {
    auto* holder = CCNode::create();
    holder->setPosition(at);
    layer->addChild(holder, 17);
    holder->setRotation(rnd(0.f, 360.f));
    for (int i = 0; i < count; ++i) {
        auto* item = sprite("ray", length, tints[i % tints.size()]);
        if (!item) break;
        item->setAnchorPoint({.5f, 0.f});
        item->setRotation(i * 360.f / count);
        float const base = item->getScale();
        item->setScale(base * .3f);
        item->setOpacity(0);
        item->runAction(CCSequence::create(
            CCDelayTime::create(delay + i * .015f),
            CCSpawn::create(CCFadeTo::create(.15f, 200), CCEaseBackOut::create(CCScaleTo::create(.3f, base)), nullptr),
            CCDelayTime::create(std::max(0.f, life - .7f)), CCFadeOut::create(.4f), nullptr));
        holder->addChild(item);
    }
    holder->runAction(CCSpawn::create(
        CCRotateBy::create(life + delay, 70.f),
        CCSequence::create(CCDelayTime::create(life + delay), CCRemoveSelf::create(), nullptr), nullptr));
}

// angle 방향으로 길게 그어지는 칼자국 모양 선.
inline void slash(CCNode* layer, CCPoint at, ccColor3B tint, float angle, float length, float delay = 0.f,
                  float life = .32f) {
    auto* item = sprite("streak", length, tint);
    if (!item) return;
    float const base = item->getScale();
    item->setRotation(-angle);
    item->setScaleX(base * .15f);
    item->setScaleY(base * .9f);
    item->setPosition(at);
    item->setOpacity(0);
    layer->addChild(item, 22);
    item->runAction(
        CCSequence::create(CCDelayTime::create(delay), CCFadeTo::create(.02f, 255),
                           CCEaseExponentialOut::create(CCScaleTo::create(life * .45f, base * 1.f, base * .9f)),
                           CCSpawn::create(CCFadeOut::create(life * .55f),
                                           CCScaleTo::create(life * .55f, base * 1.1f, base * .2f), nullptr),
                           CCRemoveSelf::create(), nullptr));
}

// at 주변 임의 위치에서 window 안에 무작위 시각에 반짝였다 사라지는 별빛.
inline void twinkles(CCNode* layer, CCPoint at, Tints const& tints, int count, float radius, float size = 9.f,
                     float window = .9f) {
    for (int i = 0; i < count; ++i) {
        auto* item = art::artSprite("sparkle.png", size * rnd(.7f, 1.3f));
        if (!item) return;
        item->setColor(tints[pick(int(tints.size()))]);
        item->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        float const a = rnd(0.f, 6.2831853f), d = radius * rnd(.55f, 1.1f);
        item->setPosition(at + CCPoint{std::cos(a) * d, std::sin(a) * d});
        float const base = item->getScale();
        item->setScale(0.f);
        layer->addChild(item, 23);
        item->runAction(
            CCSequence::create(CCDelayTime::create(rnd(0.f, window)),
                               CCEaseBackOut::create(CCScaleTo::create(.16f, base)), CCRotateBy::create(.2f, 45.f),
                               CCEaseSineIn::create(CCScaleTo::create(.22f, 0.f)), CCRemoveSelf::create(), nullptr));
    }
}

// texts의 글자들을 좌우로 벌려 위로 띄우는 글자 효과 (글자마다 시차를 둔다).
inline void words(CCNode* layer, CCPoint at, std::vector<char const*> const& texts, ccColor3B tint, float scale,
                  char const* font = "bigFont.fnt", float height = 30.f, float spreadX = 18.f) {
    int index = 0;
    for (auto const* text : texts) {
        auto* item = CCLabelBMFont::create(text, font);
        if (!item) continue;
        item->setColor(tint);
        item->setScale(scale * rnd(.85f, 1.1f));
        item->setRotation(rnd(-18.f, 18.f));
        float const dx = (index - (texts.size() - 1) / 2.f) * spreadX + rnd(-4.f, 4.f);
        launch(layer, item, at + CCPoint{dx * .4f, 4.f},
               CCEaseBackOut::create(CCMoveBy::create(.55f, {dx, height * rnd(.8f, 1.15f)})), .95f, index * .12f, .8f);
        ++index;
    }
}

// GD 자체 파티클 plist(preset)를 짧게 한 번 터뜨리는 효과. 파티클이 막힌 환경이면 아무것도 하지 않는다.
inline void gdBurst(CCNode* layer, CCPoint at, ccColor3B tint, char const* preset = "glitterEffectIcon.plist",
                    unsigned count = 14, float life = .55f, float speed = 70.f, float size = 6.f) {
    auto* particles = versus::particleSystem(preset);
    if (!particles) return;
    particles->setTotalParticles(count);
    particles->setEmitterMode(kCCParticleModeGravity);
    particles->setPositionType(kCCPositionTypeGrouped);
    particles->setPosition(at);
    particles->setPosVar({6.f, 6.f});
    particles->setDuration(.08f);
    particles->setLife(life);
    particles->setLifeVar(life * .3f);
    particles->setEmissionRate(count / .08f);
    particles->setAngle(90.f);
    particles->setAngleVar(180.f);
    particles->setSpeed(speed);
    particles->setSpeedVar(speed * .35f);
    particles->setGravity({0.f, -40.f});
    particles->setStartSize(size);
    particles->setStartSizeVar(size * .35f);
    particles->setEndSize(0.f);
    particles->setEndSizeVar(0.f);
    particles->setStartColor({tint.r / 255.f, tint.g / 255.f, tint.b / 255.f, 1.f});
    particles->setStartColorVar({.08f, .08f, .08f, 0.f});
    particles->setEndColor({1.f, 1.f, 1.f, 0.f});
    particles->setEndColorVar({0.f, 0.f, 0.f, 0.f});
    particles->setBlendAdditive(true);
    particles->setAutoRemoveOnFinish(true);
    layer->addChild(particles, 20);
    particles->resetSystem();
}

// 멤버 전용 이모트에 덧붙이는 고급 연출 (빛줄기 + 빛무리 + 링 + 별빛). accent는 이모트 색.
inline void royal(CCNode* layer, CCPoint at, ccColor3B accent) {
    auto const gold = rgb(0xffe18a);
    rays(layer, at, {accent, gold}, 10, 46.f, 1.25f);
    glow(layer, at, accent, 70.f, .7f, 0.f, 150);
    ring(layer, at, gold, 62.f, .3f, 1.2f, .55f, .05f);
    twinkles(layer, at, {gold, ccWHITE, accent}, 7, 34.f, 10.f, 1.f);
}
}

// 이모트 id(kind)에 맞는 연출을 parent의 at 위치에 재생한다.
// 임시 레이어를 만들어 2.2초 뒤 통째로 제거하고, 멤버 이모트면 마지막에 royal()을 덧붙인다.
void emoteEffect(CCNode* parent, CCPoint at, std::string const& kind, float scale = 1.f) {
    if (!parent) return;
    using namespace fx;
    auto* layer = CCNode::create();
    layer->setPosition(at);
    layer->setScale(scale);
    parent->addChild(layer, 30);
    layer->runAction(CCSequence::create(CCDelayTime::create(2.2f), CCRemoveSelf::create(), nullptr));
    CCPoint const o{0.f, 0.f};
    // 연출에 쓰는 색 팔레트와 파티 색 묶음
    auto const white = ccWHITE, gold = rgb(0xffd54a), pink = rgb(0xff6fa5), red = rgb(0xff4b4b), orange = rgb(0xff9a3c),
               blue = rgb(0x6fc6ff), cyan = rgb(0x8ff3ff), green = rgb(0x6dff9a), purple = rgb(0xb57bff),
               yellow = rgb(0xfff07a), grey = rgb(0xb8bfd0);
    Tints const party{pink, yellow, blue, green, purple, orange};
    auto const* emote = emotes::find(kind);
    bool const member = emote && emote->member;

    // 무료 이모트 6종 (like ~ fire)부터 멤버 이모트까지 id별 조합. 목록에 없으면 마지막 else의 기본 연출.
    if (kind == "like") {
        ring(layer, o, blue, 58.f);
        glow(layer, o, blue, 56.f);
        burst(layer, o, "star", {gold, white, blue}, 8, 11.f, 26.f, 40.f, .7f, 120.f);
    } else if (kind == "dislike") {
        ring(layer, o, red, 58.f);
        glow(layer, o, rgb(0xff6a5a), 54.f, .55f, 0.f, 170);
        fall(layer, o, "smoke", {grey, rgb(0xff8a7a)}, 5, 16.f, 10.f, 18.f, 40.f, .9f);
    } else if (kind == "smile") {
        glow(layer, o, yellow, 60.f);
        twinkles(layer, o, {yellow, white, gold}, 8, 30.f, 11.f, .7f);
        rise(layer, o, "heart", {yellow, gold}, 3, 9.f, 34.f, 16.f, .9f, .3f);
    } else if (kind == "sad") {
        fall(layer, o, "drop", {blue, rgb(0x9fd8ff)}, 9, 9.f, 30.f, 26.f, 54.f, .95f);
        glow(layer, o, rgb(0x4a7dff), 52.f, .8f, 0.f, 130);
    } else if (kind == "angry") {
        ring(layer, o, red, 60.f, .3f, 1.3f, .4f);
        rise(layer, o + CCPoint{0.f, 8.f}, "smoke", {rgb(0xff8a7a), grey}, 5, 18.f, 30.f, 18.f, .8f, .2f, false);
        burst(layer, o, "bolt", {red, orange}, 4, 13.f, 22.f, 34.f, .5f, 0.f, .05f);
    } else if (kind == "fire") {
        rise(layer, o + CCPoint{0.f, -6.f}, "flame", {orange, yellow, red}, 10, 17.f, 42.f, 18.f, .85f, .45f);
        gdBurst(layer, o, orange, "explodeEffect.plist", 16, .45f, 85.f, 4.f);
    } else if (kind == "money") {
        fountain(layer, o, "coin", {white}, 10, 12.f, 58.f, 38.f, 1.f);
        rise(layer, o, "star", {green, white}, 4, 8.f, 36.f, 22.f, .9f, .4f);
        words(layer, o, {"$", "$"}, green, .55f);
    } else if (kind == "heart") {
        glow(layer, o, pink, 60.f);
        rise(layer, o, "heart", {pink, red, rgb(0xffb3d1)}, 8, 11.f, 46.f, 22.f, 1.1f, .5f);
        ring(layer, o, pink, 58.f, .3f, 1.15f, .5f, .1f);
    } else if (kind == "laugh" || kind == "lol") {
        auto const tint = kind == "lol" ? orange : yellow;
        words(layer, o, {"HA", "HA", "HA"}, tint, .42f);
        burst(layer, o, "confetti", {tint, white}, 8, 7.f, 24.f, 38.f, .65f, 300.f, 0.f, 90.f, 360.f, false);
    } else if (kind == "cool") {
        slash(layer, o + CCPoint{0.f, 2.f}, white, 35.f, 70.f, .05f);
        twinkles(layer, o, {cyan, white, blue}, 8, 30.f, 10.f, .8f);
        glow(layer, o, cyan, 56.f, .6f, 0.f, 150);
    } else if (kind == "wow" || kind == "star") {
        ring(layer, o, gold, 60.f, .25f, 1.3f, .45f);
        ring(layer, o, white, 50.f, .2f, 1.1f, .45f, .12f);
        burst(layer, o, "star", {gold, yellow, white}, kind == "star" ? 10 : 8, 13.f, 28.f, 46.f, .8f, 200.f);
    } else if (kind == "cry") {
        fountain(layer, o + CCPoint{-6.f, 4.f}, "drop", {blue, cyan}, 6, 9.f, 26.f, 40.f, .85f, -1.f);
        fountain(layer, o + CCPoint{6.f, 4.f}, "drop", {blue, cyan}, 6, 9.f, 26.f, 40.f, .85f, 1.f);
    } else if (kind == "love") {
        burst(layer, o, "heart", {pink, red}, 9, 11.f, 26.f, 44.f, .8f, 60.f);
        rise(layer, o, "heart", {pink, rgb(0xffc4dc)}, 4, 9.f, 40.f, 14.f, 1.f, .5f);
        glow(layer, o, pink, 64.f);
    } else if (kind == "wink") {
        twinkles(layer, o + CCPoint{10.f, 6.f}, {pink, white}, 4, 14.f, 13.f, .4f);
        rise(layer, o, "heart", {pink}, 2, 11.f, 36.f, 10.f, .9f, .3f);
        slash(layer, o + CCPoint{8.f, 8.f}, white, 0.f, 30.f, .1f, .25f);
    } else if (kind == "tongue") {
        burst(layer, o, "bubble", {pink, white}, 7, 11.f, 22.f, 40.f, .9f, 0.f);
        glow(layer, o, pink, 50.f, .5f);
    } else if (kind == "sweat") {
        fountain(layer, o + CCPoint{10.f, 10.f}, "drop", {cyan, blue}, 5, 9.f, 18.f, 26.f, .75f, 1.f);
        glow(layer, o, blue, 48.f, .5f, 0.f, 120);
    } else if (kind == "sleep") {
        rise(layer, o + CCPoint{10.f, 8.f}, "z", {rgb(0xb8d9ff), white}, 4, 11.f, 40.f, 6.f, 1.3f, .9f);
        burst(layer, o, "bubble", {rgb(0xb8d9ff)}, 4, 8.f, 16.f, 26.f, 1.f, 0.f);
    } else if (kind == "think") {
        orbit(layer, o, "glow", {white, cyan, white}, 3, 9.f, 26.f, 1.3f, 1.f, .4f, 4.f);
        words(layer, o + CCPoint{12.f, 6.f}, {"?"}, cyan, .55f, "bigFont.fnt", 26.f);
    } else if (kind == "skull" || kind == "rip") {
        rise(layer, o, "smoke", {rgb(0x9a86c4), grey}, 7, 20.f, 34.f, 24.f, 1.f, .4f, false);
        ring(layer, o, rgb(0x8b6dd6), 56.f, .3f, 1.1f, .55f);
        if (kind == "rip") fall(layer, o, "petal", {white, rgb(0xd9d9e8)}, 7, 9.f, 30.f, 30.f, 52.f, 1.2f, 220.f);
    } else if (kind == "ghost") {
        rise(layer, o, "smoke", {white, rgb(0xd7e6ff)}, 6, 20.f, 28.f, 30.f, 1.1f, .4f, false);
        burst(layer, o, "glow", {white}, 5, 10.f, 18.f, 34.f, 1.f, 0.f);
    } else if (kind == "devil") {
        rise(layer, o + CCPoint{0.f, -6.f}, "flame", {purple, red, rgb(0xff4fd8)}, 10, 16.f, 40.f, 20.f, .85f, .45f);
        burst(layer, o, "bolt", {red, purple}, 4, 12.f, 26.f, 36.f, .45f);
    } else if (kind == "angel") {
        orbit(layer, o, "star", {gold, white}, 6, 9.f, 22.f, 1.4f, .8f, .3f, 22.f);
        fall(layer, o, "feather", {white, rgb(0xfff4c9)}, 6, 12.f, 36.f, 30.f, 56.f, 1.3f, 160.f);
        glow(layer, o, gold, 64.f, .8f, 0.f, 160);
    } else if (kind == "dizzy") {
        orbit(layer, o, "star", {yellow, gold, white}, 5, 10.f, 26.f, 1.4f, 2.f, .38f, 14.f);
    } else if (kind == "party" || kind == "gg" || kind == "wp") {
        Tints const tints = kind == "gg" ? Tints{blue, gold, white} : kind == "wp" ? Tints{green, white, gold} : party;
        fountain(layer, o, "confetti", tints, 12, 8.f, 60.f, 46.f, 1.1f);
        fall(layer, o, "confetti", tints, 10, 7.f, 38.f, 46.f, 60.f, 1.2f, 360.f);
        if (kind == "party")
            rise(layer, o, "note", {pink, yellow, cyan}, 3, 11.f, 40.f, 24.f, 1.f, .4f);
        else
            burst(layer, o, "star", {tints[0], white}, 6, 10.f, 24.f, 38.f, .7f, 150.f);
    } else if (kind == "crown" || kind == "trophy") {
        rays(layer, o, {gold, yellow}, 12, 52.f, 1.35f);
        twinkles(layer, o, {gold, white}, 8, 32.f, 11.f, 1.f);
        if (kind == "trophy")
            fountain(layer, o, "coin", {white}, 7, 11.f, 52.f, 34.f, 1.f);
        else
            burst(layer, o, "star", {gold, white}, 6, 10.f, 26.f, 40.f, .75f, 160.f);
    } else if (kind == "bolt") {
        glow(layer, o, yellow, 70.f, .35f, 0.f, 255);
        ring(layer, o, yellow, 62.f, .2f, 1.35f, .35f);
        burst(layer, o, "bolt", {yellow, white}, 7, 14.f, 28.f, 46.f, .45f, 0.f);
    } else if (kind == "gl") {
        rise(layer, o, "petal", {green, rgb(0xc9ffd8)}, 7, 10.f, 42.f, 22.f, 1.1f, .4f);
        twinkles(layer, o, {green, white}, 6, 30.f, 10.f, .9f);
    } else if (kind == "hi") {
        fountain(layer, o + CCPoint{12.f, 6.f}, "star", {yellow, white}, 6, 9.f, 34.f, 34.f, .8f, 1.f, true);
        rise(layer, o, "heart", {pink, yellow}, 3, 9.f, 34.f, 14.f, .9f, .3f);
    } else if (kind == "hundred") {
        ring(layer, o, red, 60.f, .25f, 1.3f, .45f);
        burst(layer, o, "star", {red, rgb(0xff9a9a), white}, 9, 10.f, 28.f, 44.f, .75f, 200.f);
        words(layer, o, {"100"}, red, .45f, "bigFont.fnt", 32.f);
    } else if (kind == "what") {
        words(layer, o, {"?", "!", "?"}, yellow, .5f);
        ring(layer, o, yellow, 58.f, .3f, 1.25f, .4f);
        burst(layer, o, "bolt", {yellow, orange}, 3, 11.f, 24.f, 34.f, .45f);
    } else if (kind == "eyes") {
        slash(layer, o + CCPoint{-9.f, 6.f}, white, 0.f, 26.f, .05f, .3f);
        slash(layer, o + CCPoint{9.f, 6.f}, white, 0.f, 26.f, .2f, .3f);
        twinkles(layer, o, {white, cyan}, 4, 24.f, 9.f, .6f);
    } else if (kind == "cold") {
        burst(layer, o, "snow", {cyan, white}, 7, 13.f, 26.f, 42.f, 1.f, 140.f);
        fall(layer, o, "snow", {white, cyan}, 6, 9.f, 34.f, 32.f, 54.f, 1.2f, 90.f, true);
        ring(layer, o, cyan, 58.f, .3f, 1.15f, .55f);
    } else if (kind == "hot") {
        rise(layer, o + CCPoint{0.f, -6.f}, "flame", {red, orange, yellow}, 9, 16.f, 40.f, 20.f, .85f, .45f);
        rise(layer, o + CCPoint{0.f, 10.f}, "smoke", {rgb(0xffb08a)}, 3, 18.f, 26.f, 16.f, .8f, .3f, false);
        fountain(layer, o + CCPoint{8.f, 8.f}, "drop", {cyan}, 3, 8.f, 16.f, 22.f, .7f, 1.f);
    } else if (kind == "swords") {
        slash(layer, o, white, 45.f, 74.f, 0.f, .32f);
        slash(layer, o, orange, -45.f, 74.f, .1f, .32f);
        burst(layer, o, "star", {orange, yellow, white}, 9, 9.f, 22.f, 42.f, .5f, 0.f, .18f);
        ring(layer, o, orange, 60.f, .2f, 1.25f, .4f, .16f);
    } else {
        glow(layer, o, yellow, 56.f);
        twinkles(layer, o, {yellow, white}, 6, 28.f);
    }
    // 멤버 이모트는 종류별 색 계열로 royal() 연출을 추가한다.
    if (member)
        royal(layer, o,
              kind == "money" || kind == "wp" || kind == "gl"                           ? green
              : kind == "heart" || kind == "love" || kind == "wink" || kind == "tongue" ? pink
              : kind == "cold" || kind == "cool" || kind == "cry" || kind == "gg"       ? cyan
              : kind == "devil" || kind == "skull" || kind == "ghost" || kind == "rip"  ? purple
              : kind == "hot" || kind == "hundred" || kind == "swords" || kind == "lol" ? orange
                                                                                        : gold);
}
