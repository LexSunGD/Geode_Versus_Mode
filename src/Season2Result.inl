// Season 2 랭크 매치 종료 후 결과 패널 (Season2ResultView). VersusUI.cpp에 #include되는 조각.
// 가운데는 VICTORY / DEFEAT 문구와 티어 구간 안에서 움직이는 레이팅 막대,
// 왼쪽은 맵 평가 투표, 오른쪽은 티어 이미지 / 이름 / 레이팅 상자다.
namespace s2view {
// 패널 배치 상수 (y 좌표는 패널 아래쪽 기준, 단위는 화면 유닛)와 블록 폭 / 간격.
constexpr float kPanelCut = 16.f;
constexpr float kOutcomeDrop = 31.f - kPanelCut;
constexpr float kExtraY = 67.f, kLineY = 45.f, kIconY = 58.f, kScoreY = 36.f, kNextY = 35.f;
constexpr float kVoteCaptionY = 90.f, kVoteButtonsY = 64.f + 40.f * .4f / 2.f, kVoteShift = -10.f;
constexpr float kVoteButtons = 40.f * .6f, kVoteCaption = .26f * .7f;
constexpr float kBoxFaceY = 82.f, kBoxNameY = 57.f, kBoxRatingY = 44.f, kBoxFace = 34.f;
constexpr float kVoteW = 100.f, kBoxW = 80.f, kBlockGap = 6.f, kEndRoom = 30.f;
// 티어 표시 이름: 서버가 준 이름이 없으면 티어 ID, 있으면 Season 2 표기로 변환.
inline std::string tierName(std::string const& tier, std::string const& name) {
    return name.empty() ? tier : season2TierName(name);
}
}

// 레이팅 변화 연출 뷰. 배치(placement) 중이면 "n / N"만 보여 주고 레이팅은 숨긴다.
// 배치가 방금 끝났으면(reveal) 티어 시작 점수에서 새 레이팅까지 채우며 처음 공개한다.
// 그 외에는 이전 레이팅에서 새 레이팅까지 막대가 이동하고, 티어가 바뀌면 상자가 전환된다.
class Season2ResultView final : public CCNode {
    RankedResult m_result;
    std::optional<LevelInfo> m_map;
    int m_games = 5;
    bool m_reveal = false, m_provisional = false, m_shielded = false;
    CCSize m_size;
    float m_center = 0.f, m_time = 0.f;
    float m_panelCenter = 0.f, m_voteX = 0.f, m_boxX = 0.f;
    CCLabelBMFont* m_ratingText = nullptr;
    CCNode* m_line = nullptr;
    CCLayerColor* m_fill = nullptr;
    CCSprite* m_head = nullptr;
    CCSprite* m_arrow = nullptr;
    float m_arrowSide = 1.f;
    float m_x0 = 0.f, m_x1 = 0.f, m_half = 100.f;
    double m_lo = 0., m_hi = 1., m_from = 0., m_to = 0.;
    bool m_lineDone = false;
    CCNode* m_box = nullptr;
    CCNode* m_boxFace = nullptr;
    Season2TierText* m_boxName = nullptr;
    CCLabelBMFont* m_boxRating = nullptr;
    std::string m_boxArt, m_boxText;
    bool m_settled = false;

    // 0..1 시간 진행을 ease-out(3차) 곡선으로 바꾼다.
    static float progress(float time) {
        float const t = std::clamp(time / ranked::RATING_ANIMATION_SECONDS, 0.f, 1.f);
        return 1.f - std::pow(1.f - t, 3.f);
    }
    // 패널 크기에서 세 블록(투표 / 막대 / 티어 상자)의 가로 위치를 계산하고 구성을 만든다.
    // 배치 중이 아니면 티어 표를 서버에서 받아 도착하면 막대(buildLine)를 만든다.
    bool init(RankedResult const& result, int placementRequired, std::optional<LevelInfo> map, CCSize size) {
        if (!CCNode::init()) return false;
        m_result = result;
        m_map = std::move(map);
        m_size = size;
        setContentSize(size);
        setID("season2-result"_spr);
        m_games = placementRequired > 0 ? placementRequired : ranked::PLACEMENT_MATCH_COUNT;
        m_reveal = result.placementComplete;
        m_provisional = result.placementAfter < m_games;
        m_lineDone = m_provisional;
        m_panelCenter = size.width / 2.f;
        float const fixed = s2view::kVoteW + s2view::kBoxW + 2.f * s2view::kBlockGap + s2view::kEndRoom;
        m_half = std::clamp((size.width - 8.f - fixed) / 2.f, 50.f, 100.f);
        float const middle = 2.f * m_half + s2view::kEndRoom;
        float const left = (size.width - (fixed + 2.f * m_half)) / 2.f;
        m_voteX = left + s2view::kVoteW / 2.f;
        m_center = left + s2view::kVoteW + s2view::kBlockGap + middle / 2.f;
        m_boxX = left + s2view::kVoteW + s2view::kBlockGap + middle + s2view::kBlockGap + s2view::kBoxW / 2.f;
        buildMiddle();
        buildVotes();
        buildBox();
        if (!m_provisional) {
            Service::get().fetchSeason2Tiers(
                Service::get().rankedCup(),
                [self = Weak<Season2ResultView>(this)](std::optional<Season2Tiers> tiers, std::string) {
                    auto owner = self.lock();
                    if (owner && tiers) owner->buildLine(*tiers);
                });
        }
        scheduleUpdate();
        return true;
    }
    // VICTORY / DEFEAT / DRAW 문구와 추가 안내 (듀얼은 레이팅 변화 없음, 연승 보너스,
    // 맵 경험으로 줄어든 손실). 배치 중이거나 공개 직전이면 레이팅 대신 진행 문구를 둔다.
    void buildMiddle() {
        auto const& result = m_result;
        float const top = m_size.height;
        auto* outcome = label(this,
                              result.result == "win"    ? "VICTORY"
                              : result.result == "loss" ? "DEFEAT"
                                                        : "DRAW",
                              {m_panelCenter, top - s2view::kOutcomeDrop}, .85f, m_size.width * .6f,
                              result.result == "loss" ? ccc3(255, 116, 99) : ccc3(255, 210, 125));
        float const outcomeScale = outcome->getScale();
        outcome->setScale(outcomeScale * .75f);
        outcome->runAction(CCEaseBackOut::create(CCScaleTo::create(.4f, outcomeScale)));
        m_shielded = result.result == "loss" && result.lossReduction > 0. && result.fullLoss < 0;
        std::string extra;
        ccColor3B extraColor = ccc3(255, 160, 60);
        if (result.duel) {
            extra = "NO RATING CHANGE";
            extraColor = kIce;
        } else if (result.result == "win" && result.winStreak >= 2 && result.streakBonus > 0.)
            extra = m_provisional || m_reveal
                        ? fmt::format("WIN STREAK x{}", result.winStreak)
                        : fmt::format("WIN STREAK x{}   +{:.0f} BONUS", result.winStreak, result.streakBonus);
        else if (m_shielded) {
            extra =
                fmt::format("MAP EXPERIENCE  -{}% LOSS", static_cast<int>(std::lround(result.lossReduction * 100.)));
            if (!m_provisional && !m_reveal)
                extra += fmt::format("   {:+d} > {:+.0f}", result.fullLoss, result.newRating - result.oldRating);
            extraColor = ccc3(120, 228, 255);
        }
        if (!extra.empty()) {
            auto* line = label(this, extra, {m_center, s2view::kExtraY}, .28f, m_half * 2.f, extraColor);
            line->setOpacity(0);
            line->runAction(CCSequence::create(CCDelayTime::create(.45f), CCFadeIn::create(.3f), nullptr));
        }
        if (m_provisional || m_reveal) {
            auto summary = m_provisional
                               ? fmt::format("{} / {}   Rating ----", result.placementAfter, std::max(1, m_games))
                               : fmt::format("{0} / {0}  |  REVEALING...", std::max(1, m_games));
            m_ratingText =
                label(this, summary, {m_center, s2view::kLineY}, .38f, m_half * 2.f, ccWHITE, false, "bigFont.fnt");
            m_ratingText->setID("season2-result-rating"_spr);
            rankedGradient(m_ratingText, result.newTier);
        }
    }
    // 맵 평가 투표 버튼 (맵 ID가 있고 투표 UI를 보여도 될 때만).
    void buildVotes() {
        if (!m_map || m_map->id <= 0 || !mapvotes::shown()) return;
        float const x = m_voteX + s2view::kVoteShift;
        label(this, "HOW WAS THIS MAP?", {x, s2view::kVoteCaptionY}, s2view::kVoteCaption, s2view::kVoteW, kIce, false,
              "bigFont.fnt");
        mapVoteButtons(this, m_map->id, m_map->name, {x, s2view::kVoteButtonsY}, s2view::kVoteButtons, -2, nullptr, 3);
    }
    // 오른쪽 티어 상자. 배치 중에는 Unranked / PLACEMENT와 진행 수를 보여 주고,
    // 아니면 이전 티어와 이전 레이팅으로 시작한다.
    void buildBox() {
        m_box = CCNode::create();
        m_box->setPosition({m_boxX, 0.f});
        m_box->setID("season2-result-tier"_spr);
        addChild(m_box, 2);
        m_boxName = Season2TierText::create();
        m_boxName->setPosition({0.f, s2view::kBoxNameY});
        m_box->addChild(m_boxName, 2);
        m_boxRating = label(m_box, "", {0.f, s2view::kBoxRatingY}, .36f, 76.f, ccWHITE, false, "bigFont.fnt");
        m_boxRating->setZOrder(2);
        bool const hidden = m_provisional || m_reveal;
        showTier(hidden ? std::string("Unranked") : m_result.oldTier,
                 hidden ? std::string("PLACEMENT") : s2view::tierName(m_result.oldTier, m_result.oldTierName), false);
        setBoxRating(m_provisional ? fmt::format("{} / {}", m_result.placementAfter, std::max(1, m_games))
                     : m_reveal    ? std::string("R----")
                                   : fmt::format("R{:.0f}", m_result.oldRating));
    }
    // 티어 이미지(glow / aura / face)를 상자 안에 만든다. 얼굴은 kBoxFace 크기에 맞춘다.
    CCNode* makeFace(std::string const& art) {
        auto* holder = CCNodeRGBA::create();
        holder->setCascadeOpacityEnabled(true);
        holder->setPosition({0.f, s2view::kBoxFaceY});
        m_box->addChild(holder, 1);
        rankedGlow(holder, art, {}, {64.f, 50.f});
        rankedAura(holder, art, {});
        auto* face = season2Face(holder, art, {}, .9f);
        if (face) {
            float const tall = std::max(face->getContentSize().width, face->getContentSize().height);
            if (tall > 1.f) face->setScale(s2view::kBoxFace / tall);
            face->setZOrder(2);
        }
        return holder;
    }
    // 상자의 레이팅 텍스트 갱신 (내용이 같으면 무시). 색은 현재 티어 그라디언트.
    void setBoxRating(std::string const& text) {
        if (!m_boxRating || text == m_boxRating->getString()) return;
        m_boxRating->setString(text.c_str());
        m_boxRating->limitLabelWidth(76.f, .36f, .18f);
        rankedGradient(m_boxRating, m_boxArt.empty() ? m_result.newTier : m_boxArt);
    }
    // 상자의 티어를 바꾼다. animate면 옛 이미지는 굴러 떨어지며 사라지고, 새 이미지는 크게 시작해
    // 줄어들며 안착하고 그때 이름 / 효과음 / 화면 흔들림이 나온다. 아니면 즉시 교체.
    void showTier(std::string const& art, std::string const& name, bool animate) {
        m_boxArt = art;
        m_boxText = name;
        auto* old = m_boxFace;
        m_boxFace = makeFace(art);
        auto setName = [this, name, art] {
            if (m_boxName) m_boxName->set(name, art, .34f, 76.f, false);
            if (m_boxRating) rankedGradient(m_boxRating, art);
        };
        if (!animate) {
            if (old) old->removeFromParent();
            setName();
            return;
        }
        if (old)
            old->runAction(CCSequence::create(
                CCDelayTime::create(.32f),
                CCSpawn::create(CCMoveBy::create(.5f, {-34.f, -56.f}), CCRotateBy::create(.5f, -155.f),
                                CCScaleTo::create(.5f, .35f), CCFadeOut::create(.5f), nullptr),
                CCRemoveSelf::create(), nullptr));
        auto* face = m_boxFace;
        face->setScale(2.8f);
        face->runAction(CCSequence::create(
            CCEaseExponentialIn::create(CCScaleTo::create(.32f, 1.f)),
            CallFuncExt::create([self = Weak<Season2ResultView>(this), setName] {
                auto owner = self.lock();
                if (!owner || !owner->m_box) return;
                setName();
                audio::play(audio::Cue::TierChange);
                rankedImpact(owner->m_box, {0.f, s2view::kBoxFaceY}, 26.f);
                owner->m_box->runAction(CCSequence::create(CCMoveBy::create(.04f, {3.f, 2.f}),
                                                           CCMoveBy::create(.06f, {-5.f, -3.f}),
                                                           CCMoveBy::create(.05f, {2.f, 1.f}), nullptr));
            }),
            CCEaseSineOut::create(CCScaleTo::create(.12f, 1.1f)), CCScaleTo::create(.12f, 1.f), nullptr));
    }
    // 티어 표(tiers)를 받은 뒤 레이팅 막대를 만든다. 새 티어 행(이름 -> 티어 ID -> 점수 순서로 찾음)을 구해
    // 막대 범위를 [현재 티어 최소 점수, 다음 티어 최소 점수]로 잡고 양끝에 티어 이미지와 점수를 둔다.
    // 공개 상황에서는 시작점을 티어 최소 점수로, 아니면 이전 레이팅으로 둔다.
    void buildLine(Season2Tiers const& tiers) {
        if (m_line || m_provisional) return;
        int const shown = static_cast<int>(std::lround(m_result.newRating));
        Season2TierRow const* mine = nullptr;
        Season2TierRow const* next = nullptr;
        auto const& rows = tiers.tiers;
        int index = -1;
        for (int i = 0; i < static_cast<int>(rows.size()) && index < 0; ++i)
            if (!m_result.newTierName.empty() && rows[i].name == m_result.newTierName) index = i;
        for (int i = 0; i < static_cast<int>(rows.size()) && index < 0; ++i)
            if (rows[i].tier == m_result.newTier) index = i;
        if (index >= 0) {
            mine = &rows[static_cast<size_t>(index)];
            if (index > 0) next = &rows[static_cast<size_t>(index - 1)];
        } else {
            for (auto const& tier : rows) {
                if (tier.minRating <= shown && (!mine || tier.minRating > mine->minRating)) mine = &tier;
                if (tier.minRating > shown && (!next || tier.minRating < next->minRating)) next = &tier;
            }
        }
        // 막대 범위와 이동 구간(m_from -> m_to). 최상위 티어면 상한을 레이팅 최댓값 등으로 잡는다.
        m_lo = mine ? mine->minRating : tiers.ratingMin;
        m_hi =
            next ? next->minRating
                 : std::max({static_cast<double>(tiers.ratingMax), m_result.newRating, m_result.oldRating, m_lo + 1.});
        if (!(m_hi > m_lo)) m_hi = m_lo + 1.;
        m_from = m_reveal ? m_lo : m_result.oldRating;
        m_to = m_result.newRating;
        m_x0 = m_center - m_half;
        m_x1 = m_center + m_half;
        m_line = CCNode::create();
        m_line->setID("season2-result-line"_spr);
        m_line->setVisible(!m_reveal || m_settled);
        addChild(m_line, 1);
        auto* track = CCLayerColor::create(ccc4(0, 0, 0, 160), m_half * 2.f, 5.f);
        track->setPosition({m_x0, s2view::kLineY - 2.5f});
        m_line->addChild(track);
        auto* shine = CCLayerColor::create(ccc4(255, 255, 255, 45), m_half * 2.f, 1.f);
        shine->setPosition({m_x0, s2view::kLineY + 1.5f});
        m_line->addChild(shine, 1);
        // 채움 색: 공개는 금색, 하락은 빨강, 상승은 초록. 현재 위치를 따라가는 빛 머리와 방향 화살표를 붙인다.
        bool const loss = !m_reveal && m_to < m_from;
        ccColor3B const colour = m_reveal ? ccc3(255, 205, 90) : loss ? ccc3(255, 92, 92) : ccc3(86, 245, 128);
        m_fill = CCLayerColor::create(ccc4(colour.r, colour.g, colour.b, 235), 0.f, 5.f);
        m_fill->setPosition({xOf(m_from), s2view::kLineY - 2.5f});
        m_line->addChild(m_fill, 2);
        if (!m_reveal) {
            auto* was = CCLayerColor::create(ccc4(255, 255, 255, 210), 2.f, 10.f);
            was->setPosition({xOf(m_from) - 1.f, s2view::kLineY - 5.f});
            m_line->addChild(was, 3);
        }
        if ((m_head = rankedParticle("particle_158_001.png"))) {
            m_head->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            m_head->setColor(colour);
            float const scale = 16.f / std::max(1.f, m_head->getContentSize().height);
            m_head->setScale(scale);
            m_head->setPosition({xOf(m_from), s2view::kLineY});
            m_line->addChild(m_head, 4);
            m_head->runAction(CCRepeatForever::create(
                CCSequence::create(CCScaleTo::create(.35f, scale * 1.25f), CCScaleTo::create(.35f, scale), nullptr)));
        }
        if (m_to != m_from) {
            m_arrowSide = m_to > m_from ? 1.f : -1.f;
            if ((m_arrow = loadedFrameSprite(m_reveal ? "GJ_arrow_02_001.png"
                                             : loss   ? "GJ_arrow_03_001.png"
                                                      : "GJ_arrow_01_001.png",
                                             "GJ_GameSheet03.plist"))) {
                m_arrow->setScale(12.f / std::max(1.f, m_arrow->getContentSize().height));
                m_arrow->setFlipX(m_arrowSide > 0.f);
                m_arrow->setPositionY(s2view::kLineY);
                m_line->addChild(m_arrow, 5);
            }
        }
        // 막대 양끝 표식: 티어 이미지(최상위 너머는 TOP)와 그 티어의 최소 점수.
        auto end = [this](float x, Season2TierRow const* tier, double rating, bool top) {
            if (tier) {
                if (auto* face = season2Face(m_line, tier->tier, {x, s2view::kIconY}, .2f)) {
                    float const tall = std::max(face->getContentSize().width, face->getContentSize().height);
                    if (tall > 1.f) face->setScale(13.f / tall);
                }
            } else if (top) {
                label(m_line, "TOP", {x, s2view::kIconY}, .3f, 30.f, ccc3(255, 214, 90), false, "goldFont.fnt");
            }
            auto* score = label(m_line, fmt::format("R{:.0f}", rating), {x, s2view::kScoreY}, .24f, 40.f, kIce, false,
                                "bigFont.fnt");
            if (tier) rankedGradient(score, tier->tier);
        };
        end(m_x0, mine, m_lo, false);
        end(m_x1, next, m_hi, !next);
        // 다음 티어까지 남은 점수 안내 문구. 막대 이동이 끝난 뒤 페이드인한다.
        std::string distance = "You are in the top tier";
        if (next) {
            distance =
                next->topRank > 0 && shown >= next->minRating
                    ? fmt::format("Reach the top {} for {}", next->topRank, season2TierName(next->name))
                    : fmt::format("{} more to {}", std::max(0, next->minRating - shown), season2TierName(next->name));
        }
        auto* gap = label(m_line, distance, {m_center, s2view::kNextY}, .4f, m_half * 2.f - 70.f, ccc3(255, 214, 60),
                          false, "chatFont.fnt");
        gap->setID("ranked-next-tier"_spr);
        gap->setOpacity(0);
        float const left = std::max(0.f, lineEnd() - m_time);
        gap->runAction(CCSequence::create(CCDelayTime::create(left + .2f), CCFadeIn::create(.3f), nullptr));
        m_lineDone = false;
        moveLine();
    }
    // 레이팅 -> 막대 위 x 좌표 (범위 밖은 양끝으로 제한).
    float xOf(double rating) const {
        double const t = std::clamp((rating - m_lo) / (m_hi - m_lo), 0., 1.);
        return m_x0 + static_cast<float>(t) * (m_x1 - m_x0);
    }
    // 막대 연출 총 길이(초). 공개 때는 끝에 1초 더 쓴다. lineProgress는 막대 이동 비율(0..1).
    float lineEnd() const { return ranked::RATING_ANIMATION_SECONDS + (m_reveal ? 1.f : 0.f); }
    float lineProgress() const {
        if (!m_reveal) return progress(m_time);
        float const t = std::clamp(m_time - ranked::RATING_ANIMATION_SECONDS, 0.f, 1.f);
        return 1.f - std::pow(1.f - t, 3.f);
    }
    // 현재 진행도에 맞게 채움 / 머리 / 화살표 위치를 갱신한다. 도착하는 순간(화면에 보일 때만)
    // 파동, 화살표 흔들림, 채움 번쩍임 효과를 한 번 낸다.
    void moveLine() {
        if (!m_line || !m_fill) return;
        float const p = lineProgress();
        double const now = m_from + (m_to - m_from) * p;
        float const a = xOf(m_from), b = xOf(now);
        m_fill->setPositionX(std::min(a, b));
        m_fill->setContentSize({std::abs(b - a), 5.f});
        if (m_head) m_head->setPositionX(b);
        if (m_arrow) m_arrow->setPositionX(b + m_arrowSide * (m_arrow->getScaledContentSize().width / 2.f + 4.f));
        if (p >= 1.f && !m_lineDone) {
            m_lineDone = true;
            if (!onScreen()) return;
            if (auto* wave = CCCircleWave::create(8.f, 34.f, .4f, false, true)) {
                wave->m_color = m_fill->getColor();
                wave->setPosition({b, s2view::kLineY});
                m_line->addChild(wave, 6);
            }
            if (m_arrow)
                m_arrow->runAction(CCRepeatForever::create(CCSequence::create(
                    CCEaseSineInOut::create(CCMoveBy::create(.35f, {m_arrowSide * 3.f, 0.f})),
                    CCEaseSineInOut::create(CCMoveBy::create(.35f, {-m_arrowSide * 3.f, 0.f})), nullptr)));
            m_fill->runAction(CCSequence::create(
                CCTintTo::create(.08f, 255, 255, 255),
                CCTintTo::create(.35f, m_fill->getColor().r, m_fill->getColor().g, m_fill->getColor().b), nullptr));
            if (m_head) {
                float const scale = m_head->getScale();
                m_head->runAction(CCSequence::create(CCScaleTo::create(.08f, scale * 2.2f),
                                                     CCEaseSineOut::create(CCScaleTo::create(.3f, scale)), nullptr));
            }
        }
    }
    // 자신과 모든 부모가 보이는 상태인지 (숨겨진 탭 뒤에서는 효과를 건너뛰기 위함).
    bool onScreen() {
        for (CCNode* node = this; node; node = node->getParent())
            if (!node->isVisible()) return false;
        return true;
    }
    // 시간을 진행시켜 막대와 상자 레이팅 숫자를 갱신한다. 연출이 끝나는 순간 한 번
    // 최종 레이팅 / 티어를 확정하고, 티어가 바뀌었으면 showTier로 전환한다.
    void update(float dt) override {
        if (m_settled && m_lineDone) return;
        m_time = std::min(m_time + std::clamp(dt, 0.f, .25f), lineEnd());
        float const p = progress(m_time);
        moveLine();
        if (m_provisional) {
            m_settled = true;
            return;
        }
        if (!m_reveal)
            setBoxRating(fmt::format("R{:.0f}", m_result.oldRating + (m_result.newRating - m_result.oldRating) * p));
        if (m_time < ranked::RATING_ANIMATION_SECONDS || m_settled) return;
        m_settled = true;
        if (m_line && m_reveal) m_line->setVisible(true);
        setBoxRating(fmt::format("R{:.0f}", m_result.newRating));
        std::string const name = s2view::tierName(m_result.newTier, m_result.newTierName);
        if (m_result.newTier != m_boxArt || name != m_boxText) showTier(m_result.newTier, name, onScreen());
        if (m_reveal && m_ratingText) {
            m_ratingText->stopAllActions();
            m_ratingText->runAction(CCSequence::create(CCFadeOut::create(.2f), CCHide::create(), nullptr));
        }
    }

public:
    static Season2ResultView* create(RankedResult const& result, int placementRequired, std::optional<LevelInfo> map,
                                     CCSize size) {
        auto* node = new Season2ResultView;
        if (node->init(result, placementRequired, std::move(map), size)) {
            node->autorelease();
            return node;
        }
        delete node;
        return nullptr;
    }
    // deltaAt: 막대 오른쪽 끝 근처 좌표. handlesTierChange: 배치 중이 아니면 이 뷰가 티어 변화를 직접 연출함.
    CCPoint deltaAt() const { return {m_center + m_half, s2view::kIconY + 22.f}; }
    bool handlesTierChange() const { return !m_provisional; }
};
