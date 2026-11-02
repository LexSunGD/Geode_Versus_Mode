// Ranked 화면의 꾸밈 요소 모음: 패널, 닉네임 줄, 티어 얼굴 아이콘과 이펙트(Season 1 불꽃 / Season 2 효과),
// 티어 색 팔레트와 글자 그라데이션, 티어 이름 텍스트, 충돌 연출, 맵 난이도 구간(band)별
// 배경색과 파티클(bandfx), 스크롤 배경 RankedBackground. 다른 Ranked 팝업과 씬이 가져다 쓴다.
//
// 색과 투명도를 지정하는 반투명 배경 패널을 만들어 parent에 붙인다.
NineSlice* rankedPanel(CCNode* parent, CCPoint origin, CCSize size, ccColor3B color = {43, 22, 12},
                       GLubyte opacity = 230) {
    auto* bg = NineSlice::create("square02b_001.png");
    bg->setContentSize(size);
    bg->setPosition(origin + size / 2.f);
    bg->setColor(color);
    bg->setOpacity(opacity);
    parent->addChild(bg, -1);
    return bg;
}

// 닉네임 한 줄을 가운데 정렬로 만든다: 순위 배지(상위 100위), 정확한 순위 숫자(#N),
// 이름(멤버십/치장 스타일), 관리자 표시 아이콘을 폭(maxWidth)에 맞춰 늘어놓는다.
CCNode* rankedNickname(CCNode* parent, PlayerProfile const& profile, int rank, CCPoint at, float scale,
                       float maxWidth) {
    auto* row = CCNode::create();
    row->setPosition(at);
    parent->addChild(row);
    float badgeWidth = 0.f;
    CCSprite* badge = nullptr;
    if (auto frame = ranked::rankBadgeFrame(rank)) {
        if (auto* spriteFrame = loadedFrame(frame, "GJ_GameSheet03.plist"))
            badge = CCSprite::createWithSpriteFrame(spriteFrame);
        if (badge && badge->getContentSize().height > 0.f) {
            badge->setScale(std::clamp(scale * 35.f, 8.f, 18.f) / badge->getContentSize().height);
            badgeWidth = badge->getScaledContentSize().width + 3.f;
            row->addChild(badge);
        } else
            badge = nullptr;
    }
    CCLabelBMFont* position = nullptr;
    float numberWidth = 0.f;
    if (ranked::showExactRank(rank)) {
        position = label(row, fmt::format("#{}", rank), {}, scale * .72f, maxWidth * .4f, ccc3(255, 214, 148), true);
        numberWidth = position->getScaledContentSize().width + 4.f;
    }
    CCSprite* wrench = nullptr;
    float wrenchWidth = 0.f;
    if (Service::get().isModerator(profile.uid)) {
        wrench = art::artSprite("staff-wrench.png", std::clamp(scale * 30.f, 7.f, 15.f));
        if (wrench) {
            wrenchWidth = wrench->getScaledContentSize().width + 3.f;
            wrench->setID("staff-wrench"_spr);
            row->addChild(wrench, 1);
        }
    }
    auto* name = label(row, profile.name, {}, scale, std::max(8.f, maxWidth - badgeWidth - numberWidth - wrenchWidth),
                       ccWHITE, true);
    styleNickname(name, profile.membership, profile.cosmetics);
    float left = -(badgeWidth + numberWidth + name->getScaledContentSize().width + wrenchWidth) / 2.f;
    if (badge) badge->setPosition({left + (badgeWidth - 3.f) / 2.f, 0.f});
    if (position) position->setPosition({left + badgeWidth, 0.f});
    name->setPosition({left + badgeWidth + numberWidth, 0.f});
    if (wrench)
        wrench->setPosition(
            {left + badgeWidth + numberWidth + name->getScaledContentSize().width + 3.f + (wrenchWidth - 3.f) / 2.f,
             0.f});
    return row;
}

// 티어 문자열은 "이름@N" 꼴일 수 있다(N=2~4는 특수 프레임). 이름 부분만 돌려준다.
std::string rankedArtKey(std::string const& tier) {
    auto const at = tier.find('@');
    return at == std::string::npos ? tier : tier.substr(0, at);
}
// "@N"의 N(2~4)을 프레임 번호로 읽는다. 없거나 범위 밖이면 0.
int rankedFrame(std::string const& tier) {
    auto const at = tier.find('@');
    if (at == std::string::npos || at + 2 != tier.size()) return 0;
    int const frame = tier[at + 1] - '0';
    return frame >= 2 && frame <= 4 ? frame : 0;
}

// 티어 이름에서 난이도 얼굴 아이콘 번호(diffIcon_NN)를 고른다. Unranked는 0.
int rankedFaceID(std::string const& tier) {
    if (tier.find('@') != std::string::npos) return rankedFaceID(rankedArtKey(tier));
    if (tier == "Unranked") return 0;
    if (goat::isGoat(tier)) return 10;
    if (tier == "Extreme Demon") return 10;
    if (tier == "Insane Demon") return 9;
    if (tier == "Hard Demon" || tier == "Demon") return 6;
    if (tier == "Medium Demon") return 8;
    if (tier == "Easy Demon") return 7;
    if (tier == "Insane") return 5;
    if (tier == "Harder") return 4;
    if (tier == "Hard") return 3;
    if (tier == "Normal") return 2;
    return 1;
}

// 아래에서 정의되는 이펙트 함수들의 선언.
void rankedFaceHalo(CCSprite* face, std::string const& tier, bool animate);
void rankedFaceFire(CCSprite* face, std::string const& tier);
void season2FaceEffect(CCSprite* face, std::string const& tier);
// 얼굴 그리기 룩을 강제로 Season 2 / Season 1로 바꾸는 임시 플래그(season2Face, season1Face가 켠다).
bool s_season2FaceLook = false;
bool s_season1FaceLook = false;
// 지금 Season 2 룩을 쓸지. 강제 플래그가 없으면 Service의 시즌 상태를 따른다.
bool rankedSeason2Look() {
    return s_season2FaceLook || (!s_season1FaceLook && Service::get().season2());
}

// 특수 프레임(불꽃 코인)을 얼굴 뒤에 놓을 때 얼굴 종류별로 아래로 내리는 보정값.
float rankedFeatureDrop(int faceID) {
    return faceID <= 2 || faceID == 5 ? 6.125f : faceID == 6 ? 6.f : 5.75f;
}
// 프레임 번호 2~4에 맞는 불꽃 코인 스프라이트를 얼굴 뒤에 붙인다.
void rankedFeatureFrame(CCSprite* face, int frame, int faceID) {
    static constexpr char const* frames[] = {"GJ_epicCoin_001.png", "GJ_epicCoin2_001.png", "GJ_epicCoin3_001.png"};
    if (frame < 2 || frame > 4) return;
    auto* fire = loadedFrameSprite(frames[frame - 2], "GJ_GameSheet03.plist");
    if (!fire) return;
    fire->setID("rank-face-feature"_spr);
    auto const size = face->getContentSize();
    fire->setPosition({size.width / 2.f, size.height / 2.f - rankedFeatureDrop(faceID)});
    face->addChild(fire, -1);
}

// 티어 얼굴 아이콘을 만들어 붙인다. 후광(halo)을 깔고, 특수 프레임이 있으면 붙이며,
// Season 2 룩이면 큰 크기에서만 Season 2 이펙트를, 아니면 크기와 조건에 맞을 때 Season 1 불꽃을 얹는다.
// GOAT 티어는 전용 얼굴을 쓴다.
CCSprite* rankedFace(CCNode* parent, std::string const& tier, CCPoint at, float scale) {
    auto name = fmt::format("diffIcon_{:02}_btn_001.png", rankedFaceID(tier));
    auto* frame = loadedFrame(name.c_str(), "GJ_GameSheet03.plist");
    if (goat::isGoat(rankedArtKey(tier))) {
        CCSize const box = frame ? frame->getOriginalSize() : CCSize{32.f, 32.f};
        if (auto* goatFace = goat::face(box, scale >= .55f)) {
            goatFace->setPosition(at);
            goatFace->setScale(scale);
            parent->addChild(goatFace, 4);
            rankedFaceHalo(goatFace, tier, scale >= .7f);
            return goatFace;
        }
    }
    auto* face = frame ? CCSprite::createWithSpriteFrame(frame) : CCSprite::create();
    if (face->getContentSize().width <= 0.f || face->getContentSize().height <= 0.f) face->setContentSize({1.f, 1.f});
    face->setPosition(at);
    face->setScale(scale);
    parent->addChild(face, 4);
    rankedFaceHalo(face, tier, scale >= .7f);
    int const fire = rankedFrame(tier);
    if (fire) rankedFeatureFrame(face, fire, rankedFaceID(tier));
    if (rankedSeason2Look()) {
        if (scale >= .55f) season2FaceEffect(face, tier);
        return face;
    }
    if (scale >= .6f && (!fire || goat::isGoat(tier))) rankedFaceFire(face, tier);
    return face;
}
// 항상 Season 2 룩으로 얼굴을 그린다.
CCSprite* season2Face(CCNode* parent, std::string const& tier, CCPoint at, float scale) {
    s_season2FaceLook = true;
    auto* face = rankedFace(parent, tier, at, scale);
    s_season2FaceLook = false;
    return face;
}
// 항상 Season 1 룩으로 얼굴을 그린다.
CCSprite* season1Face(CCNode* parent, std::string const& tier, CCPoint at, float scale) {
    s_season1FaceLook = true;
    auto* face = rankedFace(parent, tier, at, scale);
    s_season1FaceLook = false;
    return face;
}

// Season 2 데몬 티어 전용 팔레트. 해당하지 않으면 nullopt.
std::optional<std::vector<ccColor3B>> season2DemonPalette(std::string const& art) {
    if (art == "Easy Demon") return std::vector<ccColor3B>{ccc3(128, 58, 236), ccc3(255, 122, 214)};
    if (art == "Medium Demon") return std::vector<ccColor3B>{ccc3(255, 60, 210), ccc3(255, 70, 128)};
    if (art == "Hard Demon") return std::vector<ccColor3B>{ccc3(255, 52, 66), ccc3(255, 140, 120)};
    if (art == "Insane Demon") return std::vector<ccColor3B>{ccc3(236, 28, 28), ccc3(255, 160, 40)};
    if (art == "Extreme Demon") return std::vector<ccColor3B>{ccc3(200, 12, 22), ccc3(100, 0, 8), ccc3(255, 212, 46)};
    return std::nullopt;
}
// 팔레트 키 접두어: "s2|티어", "s1|티어" 형태로 시즌 룩을 지정해 팔레트를 요청할 수 있다.
constexpr std::string_view kSeason2PaletteKey = "s2|";
std::string season2PaletteKey(std::string const& tier) {
    return std::string(kSeason2PaletteKey) + tier;
}
constexpr std::string_view kSeason1PaletteKey = "s1|";
std::string season1PaletteKey(std::string const& tier) {
    return std::string(kSeason1PaletteKey) + tier;
}
// 티어의 글자/효과 색 목록(2~3색). 시즌 접두어와 "@N" 접미어를 처리하고,
// Season 2 룩이면 데몬 팔레트를 우선한다. 알 수 없는 티어는 기본 파랑 계열.
std::vector<ccColor3B> rankedPalette(std::string const& tier) {
    if (std::string_view(tier).substr(0, kSeason1PaletteKey.size()) == kSeason1PaletteKey) {
        bool const was = s_season1FaceLook;
        s_season1FaceLook = true;
        auto palette = rankedPalette(tier.substr(kSeason1PaletteKey.size()));
        s_season1FaceLook = was;
        return palette;
    }
    if (std::string_view(tier).substr(0, kSeason2PaletteKey.size()) == kSeason2PaletteKey) {
        auto const art = rankedArtKey(tier.substr(kSeason2PaletteKey.size()));
        if (auto own = season2DemonPalette(art)) return *own;
        return rankedPalette(art);
    }
    if (rankedSeason2Look())
        if (auto own = season2DemonPalette(rankedArtKey(tier))) return *own;
    if (tier.find('@') != std::string::npos) return rankedPalette(rankedArtKey(tier));
    if (goat::isGoat(tier)) return {ccc3(255, 222, 40), ccc3(190, 104, 90), ccc3(255, 222, 40)};
    if (tier == "Unranked") return {ccc3(165, 177, 195), ccWHITE};
    if (tier == "Easy") return {ccc3(62, 190, 255), ccWHITE};
    if (tier == "Normal") return {ccc3(75, 220, 86), ccWHITE};
    if (tier == "Hard") return {ccc3(255, 219, 55), ccWHITE};
    if (tier == "Harder") return {ccc3(255, 151, 48), ccc3(255, 236, 92)};
    if (tier == "Insane") return {ccc3(184, 89, 255), ccWHITE};
    if (tier == "Easy Demon") return {ccc3(255, 91, 91), ccc3(255, 198, 84)};
    if (tier == "Medium Demon") return {ccc3(255, 65, 75), ccc3(212, 73, 255)};
    if (tier == "Hard Demon") return {ccc3(225, 42, 55), ccc3(255, 111, 52)};
    if (tier == "Insane Demon") return {ccc3(255, 159, 43), ccc3(255, 48, 52)};
    if (tier == "Extreme Demon") return {ccc3(255, 57, 63), ccc3(65, 26, 38), ccc3(255, 155, 43)};
    return {ccc3(50, 175, 255), ccWHITE};
}

// BMFont 라벨의 글자(자식 스프라이트)마다 가로 위치에 따라 팔레트를 보간한 색을 입힌다.
void applyRankedGradient(CCLabelBMFont* text, std::string const& tier) {
    if (!text || !text->getChildren()) return;
    auto colors = rankedPalette(tier);
    float width = std::max(1.f, text->getContentSize().width);
    for (auto* child : CCArrayExt<CCNode*>(text->getChildren())) {
        auto* glyph = typeinfo_cast<CCSprite*>(child);
        if (!glyph || !glyph->isVisible()) continue;
        auto colorAt = [&](float x) {
            float t = std::clamp(x / width, 0.f, 1.f) * (colors.size() - 1);
            size_t i = std::min(static_cast<size_t>(t), colors.size() - 2);
            t -= i;
            auto a = colors[i], b = colors[i + 1];
            return ccc3(static_cast<GLubyte>(std::lerp(float(a.r), float(b.r), t)),
                        static_cast<GLubyte>(std::lerp(float(a.g), float(b.g), t)),
                        static_cast<GLubyte>(std::lerp(float(a.b), float(b.b), t)));
        };
        glyph->setColor(colorAt(glyph->boundingBox().getMidX()));
    }
}

// 라벨에 그라데이션을 유지시키는 보조 노드. 라벨 글자나 투명도가 바뀌면 다시 입히고,
// 라벨이 화면에서 빠지면 스스로 제거된다.
class RankedGradientBinding final : public CCNode {
    std::string m_tier, m_text;
    GLubyte m_opacity = 0;
    Weak<CCLabelBMFont> m_label;

public:
    void bind(CCLabelBMFont* label) { rebind(m_label, label); }
    void configure(std::string const& tier) {
        auto label = m_label.lock();
        if (!label) return;
        if (m_tier == tier && m_text == label->getString() && m_opacity == label->getDisplayedOpacity()) return;
        m_tier = tier;
        m_text = label->getString();
        m_opacity = label->getDisplayedOpacity();
        applyRankedGradient(label.data(), tier);
    }
    void update(float) override {
        auto label = m_label.lock();
        if (!label || !label->getParent()) {
            removeFromParent();
            return;
        }
        if (m_text != label->getString() || m_opacity != label->getDisplayedOpacity()) configure(m_tier);
    }
    CREATE_FUNC(RankedGradientBinding);
};
// 라벨에 티어 그라데이션을 적용한다(이미 있으면 티어만 갱신).
void rankedGradient(CCLabelBMFont* text, std::string const& tier) {
    if (!text || !text->getParent()) return;
    auto* binding = static_cast<RankedGradientBinding*>(text->getUserObject("rank-gradient"_spr));
    if (!binding) {
        binding = RankedGradientBinding::create();
        binding->setID("rank-gradient"_spr);
        binding->bind(text);
        text->setUserObject("rank-gradient"_spr, binding);
        text->getParent()->addChild(binding);
        binding->scheduleUpdate();
    }
    binding->configure(tier);
}
// 그라데이션을 떼고 글자를 흰색으로 되돌린다.
void rankedGradientOff(CCLabelBMFont* text) {
    if (!text) return;
    if (auto* binding = static_cast<CCNode*>(text->getUserObject("rank-gradient"_spr))) binding->removeFromParent();
    text->setUserObject("rank-gradient"_spr, nullptr);
    if (auto* glyphs = text->getChildren())
        for (auto* child : CCArrayExt<CCNode*>(glyphs))
            if (auto* glyph = typeinfo_cast<CCSprite*>(child)) glyph->setColor(ccWHITE);
}

// Season 2 티어 이름에서 "Epic ", "Legendary " 등 등급 접두어를 떼고 기본 이름만 남긴다.
std::string season2TierName(std::string const& name) {
    for (std::string_view word : {"Epic ", "Legendary ", "Legend ", "Mythic ", "Mystic "})
        if (name.size() > word.size() && std::string_view(name).substr(0, word.size()) == word)
            return name.substr(word.size());
    return name;
}
// 특수 프레임(2~4)별 강조 색. GOAT는 따로 정한 색을 쓴다.
ccColor3B season2FeatureColor(int frame) {
    return frame == 2 ? ccc3(255, 150, 40) : frame == 3 ? ccc3(232, 64, 246) : ccc3(70, 185, 255);
}
constexpr ccColor3B kGoatFeatureColor{255, 186, 40};
// Season 2 티어 이름 텍스트 노드. 그라데이션 글자를 그리고, 특수 프레임이나 GOAT 티어면
// 글자 주변에 떠다니는 불꽃 입자와 작은 사각 입자를 더한다. 같은 인자로 다시 set하면 아무것도 하지 않는다.
class Season2TierText final : public CCNode {
    std::string m_key;
    float m_width = 0.f;

public:
    static Season2TierText* create() {
        auto* node = new Season2TierText;
        if (!node->init()) {
            delete node;
            return nullptr;
        }
        node->autorelease();
        return node;
    }
    void set(std::string const& name, std::string const& look, float scale, float maxWidth, bool left) {
        auto key = fmt::format("{}|{}|{}|{}|{}", name, look, scale, maxWidth, left);
        if (key == m_key) return;
        m_key = std::move(key);
        removeAllChildren();
        int const frame = rankedFrame(look);
        std::string const text = s2::upper(frame ? season2TierName(name) : name);
        auto* main = label(this, text, {}, scale, maxWidth, ccWHITE, left, s2::kRankFont);
        main->setZOrder(2);
        rankedGradient(main, season2PaletteKey(look));
        m_width = main->getScaledContentSize().width;
        if (!frame) return;
        auto const colour = goat::isGoat(look) ? kGoatFeatureColor : season2FeatureColor(frame);
        float const tall = std::max(4.f, main->getScaledContentSize().height);
        float const middle = (left ? 0.f : -m_width / 2.f) + m_width / 2.f;
        if (goat::isGoat(look)) {
            if (auto* fire = goat::band({m_width * kFireWidth, tall * kFireHeight}, colour, kFireStrength)) {
                fire->setPosition({middle, tall * (kFireHeight / 2.f - kFireFoot)});
                addChild(fire, 1);
            }
        }
        if (auto* flies = versus::particleSystem("fireballEffect.plist")) {
            flies->setEmitterMode(kCCParticleModeGravity);
            flies->setPositionType(kCCPositionTypeGrouped);
            flies->setDuration(kCCParticleDurationInfinity);
            flies->setTangentialAccel(0.f);
            flies->setTangentialAccelVar(0.f);
            flies->setRadialAccel(0.f);
            flies->setRadialAccelVar(0.f);
            flies->setStartSpin(0.f);
            flies->setStartSpinVar(0.f);
            flies->setEndSpin(0.f);
            flies->setEndSpinVar(0.f);
            flies->setBlendAdditive(true);
            if (auto* round = loadedFrame("particle_158_001.png", "GJ_ParticleSheet.plist"))
                flies->setDisplayFrame(round);
            unsigned const count = static_cast<unsigned>(std::clamp(m_width / kFlySpacing, kFliesMin, kFliesMax));
            float const life = kFlyLife;
            flies->setTotalParticles(count);
            flies->setLife(life);
            flies->setLifeVar(.5f);
            flies->setEmissionRate(count / life);
            flies->setPosition({middle, 0.f});
            flies->setSourcePosition({0.f, 0.f});
            flies->setPosVar({m_width / 2.f, tall * .3f});
            flies->setAngle(90.f);
            flies->setAngleVar(35.f);
            flies->setSpeed(tall * .55f);
            flies->setSpeedVar(tall * .25f);
            flies->setGravity({0.f, tall * .2f});
            flies->setStartSize(tall * kFlySize);
            flies->setStartSizeVar(tall * .08f * kFlySizeSpread);
            flies->setEndSize(tall * .13f);
            flies->setEndSizeVar(tall * .04f * kFlySizeSpread);
            auto const bright = art::light(colour, kFlyLight);
            flies->setStartColor({bright.r / 255.f, bright.g / 255.f, bright.b / 255.f, 1.f});
            flies->setStartColorVar({0.f, 0.f, 0.f, 0.f});
            flies->setEndColor({colour.r / 255.f, colour.g / 255.f, colour.b / 255.f, 0.f});
            flies->setEndColorVar({0.f, 0.f, 0.f, 0.f});
            addChild(flies, 3);
            flies->resetSystem();
        }
        if (auto* squares = versus::particleSystem("explodeEffect.plist")) {
            float const life = kSquareLife;
            auto const count = static_cast<unsigned>(std::ceil(kSquaresPerSecond * (life + kSquareLifeVar))) + 1;
            squares->setTotalParticles(count);
            squares->setEmitterMode(kCCParticleModeGravity);
            squares->setPositionType(kCCPositionTypeGrouped);
            squares->setDuration(kCCParticleDurationInfinity);
            squares->setLife(life);
            squares->setLifeVar(kSquareLifeVar);
            squares->setEmissionRate(kSquaresPerSecond);
            squares->setPosition({middle, 0.f});
            squares->setSourcePosition({0.f, 0.f});
            squares->setPosVar({m_width / 2.f, tall * .35f});
            squares->setGravity({0.f, 0.f});
            squares->setAngle(0.f);
            squares->setAngleVar(180.f);
            squares->setSpeed(tall * .22f);
            squares->setSpeedVar(tall * .1f);
            squares->setTangentialAccel(0.f);
            squares->setTangentialAccelVar(0.f);
            squares->setRadialAccel(0.f);
            squares->setRadialAccelVar(0.f);
            squares->setStartSize(tall * kSquareSize);
            squares->setStartSizeVar(tall * .07f);
            squares->setEndSize(tall * .14f);
            squares->setEndSizeVar(tall * .04f);
            squares->setStartSpin(0.f);
            squares->setStartSpinVar(180.f);
            squares->setEndSpin(0.f);
            squares->setEndSpinVar(180.f);
            auto const light = art::light(colour, kSquareLight);
            squares->setStartColor({light.r / 255.f, light.g / 255.f, light.b / 255.f, kSquareAlpha});
            squares->setStartColorVar({0.f, 0.f, 0.f, 0.f});
            squares->setEndColor({colour.r / 255.f, colour.g / 255.f, colour.b / 255.f, 0.f});
            squares->setEndColorVar({0.f, 0.f, 0.f, 0.f});
            squares->setBlendAdditive(true);
            if (auto* texture = CCTextureCache::sharedTextureCache()->addImage("square.png", false))
                squares->setTexture(texture);
            addChild(squares, 4);
            squares->resetSystem();
        }
    }
    float width() const { return m_width; }
    // 입자 수/크기/수명/밝기 등 연출 조정값.
    static constexpr float kFlySpacing = 5.f, kFliesMin = 8.f, kFliesMax = 30.f;
    static constexpr float kFlySize = .36f, kFlyLight = .6f, kFlyLife = 2.f;
    static constexpr float kFlySizeSpread = 1.3f;
    static constexpr float kSquaresPerSecond = 6.f;
    static constexpr float kSquareLife = 1.8f, kSquareLifeVar = .5f;
    static constexpr float kSquareAlpha = .85f, kSquareSize = .26f, kSquareLight = .4f;
    static constexpr float kFireWidth = 1.08f, kFireHeight = 1.9f, kFireFoot = .5f, kFireStrength = .6f;
};

// ParticleSheet 프레임으로 스프라이트를 만든다(프레임이 없으면 빈 스프라이트).
CCSprite* rankedParticle(char const* frame) {
    auto* spriteFrame = loadedFrame(frame, "GJ_ParticleSheet.plist");
    auto* sprite = spriteFrame ? CCSprite::createWithSpriteFrame(spriteFrame) : CCSprite::create();
    if (sprite->getContentSize().width <= 0.f || sprite->getContentSize().height <= 0.f)
        sprite->setContentSize({1.f, 1.f});
    return sprite;
}

// 얼굴 뒤에 깔리는 둥근 후광. animate가 true면 밝기가 천천히 오르내린다.
void rankedFaceHalo(CCSprite* face, std::string const& tier, bool animate) {
    auto* halo = loadedFrameSprite("particle_159_001.png", "GJ_ParticleSheet.plist");
    if (!halo) return;
    halo->setID("rank-face-halo"_spr);
    auto const size = face->getContentSize();
    float const diameter = std::max(size.width, size.height) * 2.1f;
    halo->setScale(diameter / std::max(1.f, std::max(halo->getContentSize().width, halo->getContentSize().height)));
    halo->setColor(art::light(rankedPalette(tier).front(), .2f));
    halo->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
    halo->setOpacity(200);
    halo->setPosition(size / 2.f);
    face->addChild(halo, -1);
    if (animate)
        halo->runAction(
            CCRepeatForever::create(CCSequence::create(CCEaseSineInOut::create(CCFadeTo::create(1.7f, 130)),
                                                       CCEaseSineInOut::create(CCFadeTo::create(1.7f, 200)), nullptr)));
}

// 얼굴 종류별 Season 1 불꽃 세기(0~1).
float rankedHeat(std::string const& tier) {
    switch (rankedFaceID(tier)) {
    case 1: return .08f;
    case 2: return .16f;
    case 3: return .26f;
    case 4: return .36f;
    case 5: return .46f;
    case 7: return .58f;
    case 8: return .68f;
    case 6: return .78f;
    case 9: return .9f;
    case 10: return 1.f;
    default: return 0.f;
    }
}

// Season 1 불꽃 이펙트. 세기(heat)에 따라 층이 늘어난다: 불꽃 입자, (0.36 이상) 불꽃 혀,
// (0.58 이상) 얼굴 주위를 도는 입자, (0.68 이상) 불씨. 데몬 티어는 더 뜨거운 색을 쓴다.
void rankedFaceFire(CCSprite* face, std::string const& tier) {
    float const heat = rankedHeat(tier);
    if (heat < .12f) return;
    auto const size = face->getContentSize();
    float const d = std::max(size.width, size.height);
    CCPoint const center = size / 2.f;
    auto palette = rankedPalette(tier);
    bool const demon = rankedFaceID(tier) >= 6;
    ccColor3B const hot = demon ? ccc3(255, 226, 140) : art::light(palette.front(), .6f);
    ccColor3B const body = demon ? (heat >= 1.f ? ccc3(255, 96, 30) : palette.front()) : palette.front();
    auto* root = CCNode::create();
    root->setID("rank-face-fire"_spr);
    root->setPosition(center);
    face->addChild(root, -2);

    if (auto* fire = versus::particleSystem("fireballEffect.plist")) {
        unsigned const count = static_cast<unsigned>(10 + heat * 38);
        float const life = .45f + heat * .45f;
        fire->setTotalParticles(count);
        fire->setEmitterMode(kCCParticleModeGravity);
        fire->setPositionType(kCCPositionTypeGrouped);
        fire->setDuration(kCCParticleDurationInfinity);
        fire->setLife(life);
        fire->setLifeVar(life * .3f);
        fire->setEmissionRate(count / life);
        fire->setPosition({0.f, -d * .08f});
        fire->setSourcePosition({0.f, 0.f});
        fire->setPosVar({d * (.2f + heat * .12f), d * .06f});
        fire->setAngle(90.f);
        fire->setAngleVar(10.f + heat * 10.f);
        fire->setSpeed(d * (.35f + heat * .9f));
        fire->setSpeedVar(d * .2f);
        fire->setGravity({0.f, d * (.4f + heat)});
        fire->setStartSize(d * (.22f + heat * .32f));
        fire->setStartSizeVar(d * .08f);
        fire->setEndSize(d * .04f);
        fire->setEndSizeVar(0.f);
        fire->setStartColor({hot.r / 255.f, hot.g / 255.f, hot.b / 255.f, .55f + heat * .35f});
        fire->setStartColorVar({.05f, .05f, .05f, 0.f});
        fire->setEndColor({body.r / 255.f, body.g / 255.f, body.b / 255.f, 0.f});
        fire->setEndColorVar({0.f, 0.f, 0.f, 0.f});
        fire->setBlendAdditive(true);
        root->addChild(fire, 0);
        fire->resetSystem();
    }
    if (heat >= .36f) {
        int const tongues = 3 + static_cast<int>(heat * 5.f);
        for (int i = 0; i < tongues; ++i) {
            float const t = tongues == 1 ? .5f : i / static_cast<float>(tongues - 1);
            float const angle = (150.f - 120.f * t) * art::kPi / 180.f;
            auto* flame = CCDrawNode::create();
            art::Path path{{0, 20}, {3, 11},        {6, 6},         {7, 1},  {5.6f, -3.4f}, {2.6f, -6.3f},
                           {0, -7}, {-2.6f, -6.3f}, {-5.6f, -3.4f}, {-7, 1}, {-6, 6},       {-3, 11}};
            CCPoint c{0, -1};
            art::fill(flame, path, art::f4(body, .55f), &c);
            CCPoint c2{0, -2.5f};
            art::fill(flame, art::scaled(path, .55f, {0, -5}), art::f4(hot, .7f), &c2);
            flame->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            float const k = d / 40.f * (.45f + heat * .55f) * (i % 2 ? .8f : 1.f);
            flame->setPosition({std::cos(angle) * d * .36f, std::sin(angle) * d * .30f});
            flame->setRotation(90.f - angle * 180.f / art::kPi);
            flame->setScale(k);
            root->addChild(flame, -1);
            float const p = .11f + .03f * (i % 3);
            flame->runAction(CCRepeatForever::create(CCSequence::create(
                CCScaleTo::create(p, k * .86f, k * 1.18f), CCScaleTo::create(p * 1.2f, k * 1.08f, k * .84f),
                CCScaleTo::create(p * .9f, k * .95f, k * 1.1f), CCScaleTo::create(p, k, k), nullptr)));
        }
    }
    if (heat >= .58f) {
        if (auto* aura = versus::particleSystem("glitterEffectIcon.plist")) {
            unsigned const count = static_cast<unsigned>(8 + heat * 14);
            aura->setTotalParticles(count);
            aura->setEmitterMode(kCCParticleModeRadius);
            aura->setPositionType(kCCPositionTypeGrouped);
            aura->setDuration(kCCParticleDurationInfinity);
            aura->setLife(1.3f);
            aura->setLifeVar(.3f);
            aura->setEmissionRate(count / 1.3f);
            aura->setPosition({0.f, 0.f});
            aura->setPosVar({0.f, 0.f});
            aura->setAngle(90.f);
            aura->setAngleVar(180.f);
            aura->setStartRadius(d * .5f);
            aura->setStartRadiusVar(d * .06f);
            aura->setEndRadius(d * .7f);
            aura->setEndRadiusVar(d * .08f);
            aura->setRotatePerSecond(110.f + heat * 90.f);
            aura->setRotatePerSecondVar(35.f);
            aura->setStartSize(d * (.09f + heat * .05f));
            aura->setStartSizeVar(d * .03f);
            aura->setEndSize(0.f);
            aura->setEndSizeVar(0.f);
            aura->setStartColor({hot.r / 255.f, hot.g / 255.f, hot.b / 255.f, .9f});
            aura->setStartColorVar({.05f, .05f, .05f, 0.f});
            aura->setEndColor({body.r / 255.f, body.g / 255.f, body.b / 255.f, 0.f});
            aura->setEndColorVar({0.f, 0.f, 0.f, 0.f});
            aura->setBlendAdditive(true);
            root->addChild(aura, -3);
            aura->resetSystem();
        }
    }
    if (heat >= .68f) {
        if (auto* embers = versus::particleSystem("explodeEffect.plist")) {
            unsigned const count = static_cast<unsigned>(4 + heat * 10);
            embers->setTotalParticles(count);
            embers->setEmitterMode(kCCParticleModeGravity);
            embers->setPositionType(kCCPositionTypeGrouped);
            embers->setDuration(kCCParticleDurationInfinity);
            embers->setLife(1.1f);
            embers->setLifeVar(.3f);
            embers->setEmissionRate(count / 1.1f);
            embers->setPosition({0.f, 0.f});
            embers->setPosVar({d * .35f, d * .2f});
            embers->setAngle(90.f);
            embers->setAngleVar(25.f);
            embers->setSpeed(d * .7f);
            embers->setSpeedVar(d * .3f);
            embers->setGravity({0.f, d * .3f});
            embers->setStartSize(d * .07f);
            embers->setStartSizeVar(d * .03f);
            embers->setEndSize(0.f);
            embers->setEndSizeVar(0.f);
            embers->setStartSpin(0.f);
            embers->setStartSpinVar(180.f);
            embers->setEndSpin(360.f);
            embers->setEndSpinVar(180.f);
            embers->setStartColor({hot.r / 255.f, hot.g / 255.f, hot.b / 255.f, 1.f});
            embers->setStartColorVar({0.f, 0.f, 0.f, 0.f});
            embers->setEndColor({body.r / 255.f, body.g / 255.f, body.b / 255.f, 0.f});
            embers->setEndColorVar({0.f, 0.f, 0.f, 0.f});
            embers->setBlendAdditive(true);
            if (auto* texture = CCTextureCache::sharedTextureCache()->addImage("square.png", false))
                embers->setTexture(texture);
            root->addChild(embers, 1);
            embers->resetSystem();
        }
    }
}

// Season 2 얼굴 이펙트. 특수 프레임이 있으면 프레임 색에 맞는 불꽃을, 모든 티어에 떠다니는 입자를 더한다.
// Unranked는 건너뛴다.
void season2FaceEffect(CCSprite* face, std::string const& tier) {
    if (rankedArtKey(tier) == "Unranked") return;
    auto const size = face->getContentSize();
    float const d = std::max(size.width, size.height);
    CCPoint const center = size / 2.f;
    auto const tint = rankedPalette(tier).front();
    auto const glow = art::light(tint, .5f);
    auto still = [](CCParticleSystemQuad* system) {
        system->setEmitterMode(kCCParticleModeGravity);
        system->setPositionType(kCCPositionTypeGrouped);
        system->setDuration(kCCParticleDurationInfinity);
        system->setTangentialAccel(0.f);
        system->setTangentialAccelVar(0.f);
        system->setRadialAccel(0.f);
        system->setRadialAccelVar(0.f);
        system->setStartSpin(0.f);
        system->setStartSpinVar(0.f);
        system->setEndSpin(0.f);
        system->setEndSpinVar(0.f);
        system->setBlendAdditive(true);
    };
    int const feature = rankedFrame(tier);
    if (feature) {
        ccColor3B const hot = feature == 2   ? ccc3(255, 228, 120)
                              : feature == 3 ? ccc3(240, 170, 255)
                                             : ccc3(175, 240, 255);
        ccColor3B const body = feature == 2   ? ccc3(255, 92, 22)
                               : feature == 3 ? ccc3(150, 40, 255)
                                              : ccc3(40, 115, 255);
        float const heat = feature == 2 ? 1.f : feature == 3 ? 1.15f : 1.3f;
        if (auto* fire = versus::particleSystem("fireballEffect.plist")) {
            still(fire);
            unsigned const count = static_cast<unsigned>(26.f * heat);
            float const life = .75f;
            fire->setTotalParticles(count);
            fire->setLife(life);
            fire->setLifeVar(.25f);
            fire->setEmissionRate(count / life);
            fire->setPosition({center.x, center.y - d * .2f});
            fire->setSourcePosition({0.f, 0.f});
            fire->setPosVar({d * .4f, d * .12f});
            fire->setAngle(90.f);
            fire->setAngleVar(14.f);
            fire->setSpeed(d * .8f * heat);
            fire->setSpeedVar(d * .25f);
            fire->setGravity({0.f, d * .55f});
            fire->setStartSize(d * .42f);
            fire->setStartSizeVar(d * .1f);
            fire->setEndSize(d * .06f);
            fire->setEndSizeVar(0.f);
            fire->setStartColor({hot.r / 255.f, hot.g / 255.f, hot.b / 255.f, .8f});
            fire->setStartColorVar({0.f, 0.f, 0.f, 0.f});
            fire->setEndColor({body.r / 255.f, body.g / 255.f, body.b / 255.f, 0.f});
            fire->setEndColorVar({0.f, 0.f, 0.f, 0.f});
            face->addChild(fire, -3);
            fire->resetSystem();
        }
    }
    if (auto* flies = versus::particleSystem("fireballEffect.plist")) {
        still(flies);
        if (auto* round = loadedFrame("particle_158_001.png", "GJ_ParticleSheet.plist")) flies->setDisplayFrame(round);
        constexpr unsigned count = 14;
        float const life = 2.4f;
        flies->setTotalParticles(count);
        flies->setLife(life);
        flies->setLifeVar(.8f);
        flies->setEmissionRate(count / life);
        flies->setPosition(center);
        flies->setSourcePosition({0.f, 0.f});
        flies->setPosVar({d * .66f, d * .5f});
        flies->setAngle(90.f);
        flies->setAngleVar(180.f);
        flies->setSpeed(d * .14f);
        flies->setSpeedVar(d * .08f);
        flies->setGravity({0.f, d * .05f});
        flies->setStartSize(d * .2f);
        flies->setStartSizeVar(d * .06f);
        flies->setEndSize(d * .08f);
        flies->setEndSizeVar(d * .03f);
        flies->setStartColor({glow.r / 255.f, glow.g / 255.f, glow.b / 255.f, 1.f});
        flies->setStartColorVar({0.f, 0.f, 0.f, 0.f});
        flies->setEndColor({tint.r / 255.f, tint.g / 255.f, tint.b / 255.f, 0.f});
        flies->setEndColorVar({0.f, 0.f, 0.f, 0.f});
        face->addChild(flies, 2);
        flies->resetSystem();
    }
}

// 티어 색으로 천천히 깜빡이는 넓은 광채.
CCSprite* rankedGlow(CCNode* parent, std::string const& tier, CCPoint at, CCSize size) {
    auto* glow = rankedParticle("particle_158_001.png");
    glow->setPosition(at);
    glow->setColor(rankedPalette(tier).front());
    glow->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
    glow->setScaleX(size.width / glow->getContentSize().width);
    glow->setScaleY(size.height / glow->getContentSize().height);
    glow->setOpacity(100);
    parent->addChild(glow, 0);
    glow->runAction(
        CCRepeatForever::create(CCSequence::create(CCEaseSineInOut::create(CCFadeTo::create(1.8f, 145)),
                                                   CCEaseSineInOut::create(CCFadeTo::create(1.8f, 80)), nullptr)));
    return glow;
}

// 티어 이름 옆에 떠오르는 작은 입자들. 데몬 이상은 더 크고 옆으로 퍼지며, 상위 티어일수록 개수가 많다.
CCNode* rankedAura(CCNode* parent, std::string const& tier, CCPoint at) {
    auto* root = CCNode::create();
    root->setPosition(at);
    parent->addChild(root, 1);
    auto palette = rankedPalette(tier);
    auto tint = palette.front();
    int kind = rankedFaceID(tier);
    int count = kind >= 9 ? 8 : 5;
    for (int i = 0; i < count; ++i) {
        auto* mote = rankedParticle("particle_158_001.png");
        float radius = kind >= 6 ? 2.2f : 1.5f;
        mote->setColor(tint);
        float scale = radius * 3.f / mote->getContentSize().width;
        mote->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        CCPoint start{-22.f + std::fmod(i * 17.f, 44.f), -12.f + std::fmod(i * 11.f, 22.f)};
        CCPoint travel{kind >= 6 ? ((i % 2) ? 13.f : -13.f) : 0.f, 18.f + std::fmod(i * 7.f, 20.f)};
        mote->setPosition(start);
        mote->setOpacity(0);
        mote->setScale(scale);
        root->addChild(mote);
        mote->runAction(CCRepeatForever::create(CCSequence::create(
            CCDelayTime::create(i * .13f),
            CCSpawn::create(CCSequence::create(CCFadeTo::create(.22f, 190), CCFadeOut::create(1.15f), nullptr),
                            CCEaseSineOut::create(CCMoveBy::create(1.37f, travel)),
                            CCEaseSineOut::create(CCScaleTo::create(1.37f, scale * (kind >= 6 ? 1.8f : .45f))),
                            nullptr),
            versus::placeAction(start), CCScaleTo::create(0.f, scale), CCDelayTime::create(.55f), nullptr)));
    }
    return root;
}

// 타이틀 글자를 가운데는 흰색, 양쪽 끝은 주황빛으로 칠한다.
void rankedTitleGradient(CCLabelBMFont* title) {
    if (!title || !title->getChildren()) return;
    float width = std::max(1.f, title->getContentSize().width);
    for (auto* child : CCArrayExt<CCNode*>(title->getChildren())) {
        if (auto* glyph = typeinfo_cast<CCSprite*>(child)) {
            float edge = std::clamp(std::abs(child->getPositionX() / width * 2.f - 1.f), 0.f, 1.f);
            glyph->setColor(ccc3(255, static_cast<GLubyte>(255 - 116 * edge), static_cast<GLubyte>(255 - 222 * edge)));
        }
    }
}

// 충격 연출: 커지며 사라지는 링과 사방으로 튀는 사각 불꽃.
void rankedImpact(CCNode* parent, CCPoint at, float radius = 60.f) {
    auto* ring = CCDrawNode::create();
    constexpr int count = 28;
    for (int i = 0; i < count; ++i) {
        float a = i * 6.2831853f / count, b = (i + 1) * 6.2831853f / count;
        ring->drawSegment({std::cos(a) * radius, std::sin(a) * radius}, {std::cos(b) * radius, std::sin(b) * radius},
                          1.8f, {1.f, .72f, .3f, 1.f});
    }
    ring->setPosition(at);
    ring->setScale(.12f);
    parent->addChild(ring, 30);
    ring->runAction(CCSequence::create(
        CCSpawn::create(CCEaseExponentialOut::create(CCScaleTo::create(.5f, 1.6f)), CCFadeOut::create(.5f), nullptr),
        CCRemoveSelf::create(), nullptr));
    for (int i = 0; i < 18; ++i) {
        auto* spark = CCDrawNode::create();
        CCPoint corners[] = {{-2, -2}, {2, -2}, {2, 2}, {-2, 2}};
        spark->drawPolygon(corners, 4, {1.f, .82f, .48f, 1.f}, 0, {1, 1, 1, 1});
        spark->setPosition(at);
        parent->addChild(spark, 31);
        float angle = i * 6.2831853f / 18.f;
        CCPoint offset{std::cos(angle) * radius * (1.f + (i % 3) * .4f),
                       std::sin(angle) * radius * (1.f + (i % 3) * .4f)};
        spark->runAction(
            CCSequence::create(CCSpawn::create(CCEaseExponentialOut::create(CCMoveBy::create(.55f, offset)),
                                               CCRotateBy::create(.55f, 180.f), CCFadeOut::create(.55f), nullptr),
                               CCRemoveSelf::create(), nullptr));
    }
}

// 맵 난이도 구간(band)별 화면 색 구성: 배경, 타일, 패널, 강조 색, 광채 색.
struct RankedBandTheme {
    ccColor3B base, tile, root, panel;
    ccColor3B accent, glow;
};
// band 이름에 따른 색 구성. 알 수 없으면 기본 갈색 계열.
inline RankedBandTheme rankedBandTheme(std::string const& band) {
    if (band == "Easy")
        return {ccc3(7, 24, 40), ccc3(28, 88, 132),  ccc3(26, 82, 124),
                ccc3(9, 28, 46), ccc3(96, 205, 255), ccc3(225, 246, 255)};
    if (band == "Normal")
        return {ccc3(7, 31, 13), ccc3(32, 108, 46),   ccc3(30, 100, 42),
                ccc3(9, 34, 15), ccc3(110, 236, 120), ccc3(220, 255, 205)};
    if (band == "Hard")
        return {ccc3(40, 30, 4), ccc3(140, 108, 14), ccc3(136, 104, 12),
                ccc3(44, 33, 6), ccc3(255, 222, 72), ccc3(255, 250, 205)};
    if (band == "Harder")
        return {ccc3(42, 11, 9),  ccc3(136, 44, 30),  ccc3(132, 40, 28),
                ccc3(46, 13, 10), ccc3(255, 128, 72), ccc3(255, 214, 120)};
    if (band == "Insane")
        return {ccc3(26, 10, 42), ccc3(86, 40, 138),   ccc3(84, 36, 132),
                ccc3(29, 12, 46), ccc3(196, 112, 255), ccc3(255, 190, 250)};
    if (band == "Easy Demon")
        return {ccc3(40, 8, 22),  ccc3(130, 30, 66),  ccc3(128, 28, 60),
                ccc3(44, 10, 24), ccc3(255, 96, 120), ccc3(255, 205, 120)};
    if (band == "Medium Demon")
        return {ccc3(34, 8, 40), ccc3(112, 28, 124), ccc3(110, 26, 118),
                ccc3(37, 9, 43), ccc3(232, 90, 255), ccc3(255, 150, 190)};
    if (band == "Hard Demon")
        return {ccc3(40, 5, 8), ccc3(130, 20, 28), ccc3(128, 18, 26),
                ccc3(44, 6, 9), ccc3(255, 70, 70), ccc3(255, 170, 90)};
    if (band == "Insane Demon")
        return {ccc3(12, 8, 30),  ccc3(44, 30, 108),  ccc3(46, 28, 104),
                ccc3(16, 10, 38), ccc3(255, 120, 40), ccc3(255, 226, 120)};
    if (band == "Extreme Demon")
        return {ccc3(14, 3, 6), ccc3(70, 10, 18),  ccc3(78, 12, 20),
                ccc3(18, 4, 7), ccc3(255, 46, 58), ccc3(255, 150, 60)};
    if (band == "Placement")
        return {ccc3(5, 30, 34), ccc3(22, 104, 112), ccc3(20, 98, 106),
                ccc3(7, 34, 38), ccc3(90, 240, 230), ccc3(220, 255, 250)};
    return {ccc3(38, 20, 10), ccc3(108, 57, 25), ccc3(112, 55, 22), ccc3(43, 22, 12), ccc3(255, 190, 110), ccWHITE};
}
// 두 색을 t(0~1)로 섞는다.
inline ccColor3B mixColor(ccColor3B a, ccColor3B b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    return ccc3(static_cast<GLubyte>(std::lerp(float(a.r), float(b.r), t)),
                static_cast<GLubyte>(std::lerp(float(a.g), float(b.g), t)),
                static_cast<GLubyte>(std::lerp(float(a.b), float(b.b), t)));
}

// 매치 정보에서 배경 band를 정한다: 서버가 준 mapBand(배치 / 티어)를 우선하고,
// 없으면 맵 풀 마스크 값으로 판단한다. 알 수 없으면 빈 문자열.
inline std::string rankedBandKey(RankedMatch const& match) {
    if (match.mapBand) {
        if (match.mapBand->reason == "placement") return "Placement";
        if (!match.mapBand->tier.empty()) {
            auto const key = rankedArtKey(match.mapBand->tier);
            return goat::isGoat(key) ? std::string("Extreme Demon") : key;
        }
    }
    switch (match.mapMask) {
    case 6144: return "Extreme Demon";
    case 7168: return "Insane Demon";
    case 3584: return "Hard Demon";
    case 1792: return "Medium Demon";
    case 896: return "Easy Demon";
    case 448: return "Insane";
    case 224: return "Harder";
    case 120: return "Hard";
    case 60: return "Normal";
    case 28: return "Easy";
    case 248: return "Placement";
    default: return "";
    }
}

// band별 배경 파티클. Layer는 한 층의 스프라이트 종류, 움직임, 개수, 크기, 투명도,
// 지속 시간, 이동 거리, 색 사용 방식을 정의한다.
namespace bandfx {
enum class Motion { Rise, Fall, Twinkle, Float, Flash, Shoot };
enum class Tint { Accent, Glow, Both, Smoke };
struct Layer {
    char const* sprite;
    Motion motion;
    int count;
    float minSize, maxSize;
    GLubyte opacity;
    bool additive;
    float minTime, maxTime;
    float travel;
    Tint tint;
};
// band에 해당하는 층 목록.
inline std::vector<Layer> layers(std::string const& band) {
    using M = Motion;
    using T = Tint;
    Layer const glowFloat{"glow", M::Float, 6, 40.f, 72.f, 42, true, 6.f, 10.f, 0.f, T::Accent};
    Layer const embers{"glow", M::Rise, 18, 3.f, 6.f, 230, true, 3.5f, 6.5f, .75f, T::Both};
    Layer const smoke{"smoke", M::Rise, 5, 44.f, 76.f, 46, false, 9.f, 14.f, .7f, T::Smoke};
    if (band == "Easy") return {{"bubble", M::Rise, 16, 6.f, 13.f, 130, true, 7.f, 12.f, 1.1f, T::Both}, glowFloat};
    if (band == "Normal") return {{"petal", M::Fall, 14, 7.f, 12.f, 175, false, 7.f, 11.f, 1.1f, T::Both}, glowFloat};
    if (band == "Hard") return {{"star", M::Twinkle, 20, 6.f, 13.f, 225, true, 1.6f, 3.2f, 0.f, T::Both}, glowFloat};
    if (band == "Harder")
        return {{"glow", M::Rise, 24, 3.f, 6.f, 235, true, 3.5f, 6.5f, .8f, T::Both},
                {"smoke", M::Rise, 5, 44.f, 76.f, 40, false, 9.f, 14.f, .7f, T::Smoke},
                glowFloat};
    if (band == "Insane")
        return {{"star", M::Twinkle, 14, 6.f, 12.f, 225, true, 1.8f, 3.4f, 0.f, T::Both},
                {"glow", M::Float, 8, 40.f, 80.f, 52, true, 6.f, 10.f, 0.f, T::Both},
                {"streak", M::Shoot, 3, 34.f, 48.f, 210, true, 2.f, 5.f, 0.f, T::Glow}};
    if (band == "Easy Demon")
        return {{"flame", M::Rise, 12, 10.f, 18.f, 160, true, 4.5f, 8.f, .45f, T::Both}, smoke, embers};
    if (band == "Medium Demon")
        return {{"bolt", M::Flash, 5, 18.f, 30.f, 235, true, 1.5f, 4.5f, 0.f, T::Accent},
                {"glow", M::Float, 8, 40.f, 80.f, 55, true, 6.f, 10.f, 0.f, T::Both},
                embers};
    if (band == "Hard Demon")
        return {{"flame", M::Rise, 16, 12.f, 20.f, 175, true, 4.f, 7.f, .5f, T::Both}, embers, smoke};
    if (band == "Insane Demon")
        return {{"flame", M::Rise, 20, 12.f, 24.f, 195, true, 3.5f, 6.5f, .55f, T::Both},
                {"glow", M::Rise, 24, 3.f, 6.f, 235, true, 3.f, 5.5f, .85f, T::Both},
                {"streak", M::Shoot, 5, 32.f, 50.f, 220, true, 1.5f, 4.f, 0.f, T::Glow}};
    if (band == "Extreme Demon")
        return {{"flame", M::Rise, 20, 14.f, 26.f, 205, true, 3.f, 6.f, .6f, T::Both},
                {"glow", M::Rise, 26, 3.f, 6.f, 240, true, 3.f, 5.f, .9f, T::Both},
                {"bolt", M::Flash, 6, 20.f, 34.f, 240, true, 1.2f, 3.5f, 0.f, T::Accent},
                {"smoke", M::Rise, 8, 50.f, 90.f, 70, false, 8.f, 13.f, .8f, T::Smoke}};
    if (band == "Placement")
        return {{"ring", M::Twinkle, 8, 12.f, 22.f, 150, true, 2.f, 3.6f, 0.f, T::Accent},
                {"star", M::Twinkle, 12, 6.f, 11.f, 210, true, 1.6f, 3.f, 0.f, T::Glow},
                glowFloat};
    return {};
}

// band의 층 정의대로 스프라이트를 만들어 움직임(상승/하강/반짝임/부유/번쩍임/유성)을
// 반복 액션으로 건다. 처음에는 임의의 진행도에서 시작해 한꺼번에 몰려 나오지 않게 한다.
inline void spawn(CCNode* layer, std::string const& band, CCSize size) {
    auto const theme = rankedBandTheme(band);
    auto random = [](float low, float high) { return fx::rnd(low, high); };
    auto randomPoint = [size, random] {
        return CCPoint{random(8.f, size.width - 8.f), random(8.f, size.height - 8.f)};
    };
    for (auto const& spec : layers(band)) {
        for (int i = 0; i < spec.count; ++i) {
            float const scale = random(spec.minSize, spec.maxSize);
            ccColor3B color = spec.tint == Tint::Accent  ? theme.accent
                              : spec.tint == Tint::Glow  ? theme.glow
                              : spec.tint == Tint::Smoke ? mixColor(theme.panel, ccBLACK, .35f)
                                                         : (i % 2 ? theme.glow : theme.accent);
            auto* mote = fx::sprite(spec.sprite, scale, color, spec.additive);
            if (!mote) continue;
            float const base = mote->getScale();
            float const time = random(spec.minTime, spec.maxTime);
            mote->setOpacity(0);
            layer->addChild(mote);
            switch (spec.motion) {
            case Motion::Rise:
            case Motion::Fall: {
                bool const rise = spec.motion == Motion::Rise;
                float const x = random(0.f, size.width);
                float const travel = size.height * spec.travel + scale * 2.f;
                float const startY = rise ? -scale : size.height + scale;
                CCPoint const step{rise ? random(-26.f, 26.f) : -random(40.f, 90.f), rise ? travel : -travel};
                auto cycle = [=]() -> CCFiniteTimeAction* {
                    return CCSequence::create(
                        versus::placeAction({x, startY}), CCFadeTo::create(.35f, spec.opacity),
                        CCSpawn::create(CCMoveBy::create(time, step),
                                        CCSequence::create(CCDelayTime::create(time * .55f),
                                                           CCFadeOut::create(time * .45f), nullptr),
                                        CCRotateBy::create(time, rise ? random(-40.f, 40.f) : random(-260.f, 260.f)),
                                        nullptr),
                        CCDelayTime::create(random(0.f, 1.2f)), nullptr);
                };
                float const done = random(0.f, .9f);
                mote->setPosition({x + step.x * done, startY + step.y * done});
                mote->runAction(CCSequence::create(
                    CCFadeTo::create(.5f, static_cast<GLubyte>(spec.opacity * (1.f - done * .6f))),
                    CCSpawn::create(CCMoveBy::create(time * (1.f - done), step * (1.f - done)),
                                    CCSequence::create(CCDelayTime::create(time * (1.f - done) * .5f),
                                                       CCFadeOut::create(time * (1.f - done) * .5f), nullptr),
                                    nullptr),
                    CallFuncExt::create([mote, cycle] {
                        mote->runAction(CCRepeatForever::create(static_cast<CCActionInterval*>(cycle())));
                    }),
                    nullptr));
                break;
            }
            case Motion::Twinkle: {
                mote->setPosition(randomPoint());
                mote->setScale(base * .3f);
                mote->runAction(CCRepeatForever::create(CCSequence::create(
                    CCDelayTime::create(random(0.f, time)),
                    CCSpawn::create(CCFadeTo::create(time * .3f, spec.opacity),
                                    CCEaseSineOut::create(CCScaleTo::create(time * .3f, base)),
                                    CCRotateBy::create(time * .3f, 40.f), nullptr),
                    CCDelayTime::create(time * .2f),
                    CCSpawn::create(CCFadeOut::create(time * .4f), CCScaleTo::create(time * .4f, base * .3f), nullptr),
                    CallFuncExt::create([mote, randomPoint] { mote->setPosition(randomPoint()); }), nullptr)));
                break;
            }
            case Motion::Float: {
                mote->setPosition(randomPoint());
                CCPoint const drift{random(-34.f, 34.f), random(-22.f, 22.f)};
                mote->runAction(CCRepeatForever::create(
                    CCSequence::create(CCEaseSineInOut::create(CCMoveBy::create(time, drift)),
                                       CCEaseSineInOut::create(CCMoveBy::create(time, -drift)), nullptr)));
                mote->runAction(CCRepeatForever::create(
                    CCSequence::create(CCFadeTo::create(time * .8f, spec.opacity),
                                       CCFadeTo::create(time * .8f, spec.opacity / 3), nullptr)));
                break;
            }
            case Motion::Flash: {
                mote->runAction(CCRepeatForever::create(CCSequence::create(
                    CCDelayTime::create(time + random(0.f, 2.f)), CallFuncExt::create([mote, randomPoint, random] {
                        mote->setPosition(randomPoint());
                        mote->setRotation(random(-30.f, 30.f));
                    }),
                    CCFadeTo::create(.04f, spec.opacity), CCFadeTo::create(.07f, spec.opacity / 3),
                    CCFadeTo::create(.04f, spec.opacity), CCFadeOut::create(.3f), nullptr)));
                break;
            }
            case Motion::Shoot: {
                CCPoint const travel{random(90.f, 150.f), size.height * .8f};
                mote->setRotation(-std::atan2(travel.y, travel.x) * 180.f / 3.14159265f);
                float const x = random(-40.f, size.width * .8f);
                mote->runAction(CCRepeatForever::create(CCSequence::create(
                    CCDelayTime::create(time + random(0.f, 2.f)), versus::placeAction({x, -20.f}),
                    CCFadeTo::create(.05f, spec.opacity),
                    CCSpawn::create(CCEaseSineIn::create(CCMoveBy::create(.75f, travel)),
                                    CCSequence::create(CCDelayTime::create(.45f), CCFadeOut::create(.3f), nullptr),
                                    nullptr),
                    nullptr)));
                break;
            }
            }
        }
    }
}
}

// Ranked 화면 배경. 가로로 천천히 흐르는 타일 배경 위에 기본 입자 또는 band별 파티클을 얹고,
// band가 바뀌면 색을 0.7초에 걸쳐 섞는다.
class RankedBackground final : public CCLayer {
    CCLayerColor* m_base = nullptr;
    CCNode* m_tiles = nullptr;
    CCNode* m_motes = nullptr;
    CCNode* m_bandLayer = nullptr;
    float m_tileWidth = 0.f;
    std::string m_band;
    RankedBandTheme m_from = rankedBandTheme(""), m_to = rankedBandTheme("");
    float m_blend = 1.f;
    void paint(RankedBandTheme const& from, RankedBandTheme const& to, float t) {
        m_base->setColor(mixColor(from.base, to.base, t));
        auto const tile = mixColor(from.tile, to.tile, t);
        for (auto* child : CCArrayExt<CCNode*>(m_tiles->getChildren()))
            if (auto* sprite = typeinfo_cast<CCSprite*>(child)) sprite->setColor(tile);
    }

public:
    bool init() override {
        if (!CCLayer::init()) return false;
        auto size = CCDirector::sharedDirector()->getWinSize();
        m_base = CCLayerColor::create(ccc4(38, 20, 10, 255));
        addChild(m_base);
        m_tiles = CCNode::create();
        addChild(m_tiles);
        if (auto* first = CCSprite::create("game_bg_20_001.png");
            first && first->getContentSize().width > 0.f && first->getContentSize().height > 0.f && size.height > 0.f) {
            float scale = size.height / first->getContentSize().height;
            m_tileWidth = first->getContentSize().width * scale;
            int count = std::clamp(static_cast<int>(std::ceil(size.width / m_tileWidth)) + 2, 2, 32);
            for (int i = -1; i < count - 1; ++i) {
                auto* tile = i == -1 ? first : CCSprite::create("game_bg_20_001.png");
                if (!tile) continue;
                tile->setScale(scale);
                tile->setColor(ccc3(108, 57, 25));
                tile->setOpacity(145);
                tile->setPosition({(i + .5f) * m_tileWidth, size.height / 2.f});
                m_tiles->addChild(tile);
            }
        }
        m_motes = CCNode::create();
        addChild(m_motes);
        for (int i = 0; i < 18; ++i) {
            auto* mote = rankedParticle("particle_00_001.png");
            float extent = (16.f + (i % 5) * 8.f) * .3f;
            float scale = extent / mote->getContentSize().width;
            float x = 12.f + std::fmod(i * 79.f, std::max(1.f, size.width - 24.f));
            float y = 12.f + std::fmod(i * 47.f, std::max(1.f, size.height - 24.f));
            mote->setPosition({x, y});
            mote->setColor(ccWHITE);
            mote->setOpacity(0);
            mote->setScale(scale * .35f);
            m_motes->addChild(mote);
            mote->runAction(CCRepeatForever::create(CCSequence::create(
                CCDelayTime::create(i * .17f),
                CCSpawn::create(
                    CCSequence::create(CCFadeTo::create(1.2f, 26 + (i % 6) * 10), CCFadeOut::create(3.6f), nullptr),
                    CCEaseSineOut::create(CCScaleTo::create(4.8f, scale)), CCMoveBy::create(4.8f, {7.f, 12.f}),
                    nullptr),
                versus::placeAction({x, y}), CCScaleTo::create(0.f, scale * .35f), CCDelayTime::create(.8f), nullptr)));
        }
        scheduleUpdate();
        return true;
    }
    // band를 바꾼다: 현재 색에서 새 색으로 전환을 시작하고 파티클 층을 교체한다.
    // band가 비어 있으면 기본 입자만 보인다.
    void setBand(std::string const& band) {
        if (band == m_band) return;
        auto const now = m_blend >= 1.f ? m_to
                                        : RankedBandTheme{mixColor(m_from.base, m_to.base, m_blend),
                                                          mixColor(m_from.tile, m_to.tile, m_blend),
                                                          m_to.root,
                                                          m_to.panel,
                                                          m_to.accent,
                                                          m_to.glow};
        m_band = band;
        m_from = now;
        m_to = rankedBandTheme(band);
        m_blend = 0.f;
        if (m_bandLayer) {
            m_bandLayer->removeFromParent();
            m_bandLayer = nullptr;
        }
        m_motes->setVisible(band.empty());
        if (!band.empty()) {
            m_bandLayer = CCNode::create();
            m_bandLayer->setID("ranked-band-particles"_spr);
            addChild(m_bandLayer);
            bandfx::spawn(m_bandLayer, band, CCDirector::sharedDirector()->getWinSize());
        }
    }
    // 색 전환을 진행하고 타일 배경을 오른쪽으로 흘려 반복시킨다.
    void update(float dt) override {
        if (!std::isfinite(dt)) return;
        if (m_blend < 1.f) {
            m_blend = std::min(1.f, m_blend + std::clamp(dt, 0.f, .1f) / .7f);
            paint(m_from, m_to, m_blend);
        }
        if (m_tileWidth <= 0) return;
        float x = m_tiles->getPositionX() + 14.f * std::clamp(dt, 0.f, .1f);
        if (x >= m_tileWidth) x -= m_tileWidth;
        m_tiles->setPositionX(x);
    }
    CREATE_FUNC(RankedBackground);
};
