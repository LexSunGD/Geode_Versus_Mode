// Ranked 레이팅 점수 표 팝업(RATING POINTS). K값 구간별로 상대 레이팅 차이에 따른 승/패 변동량을
// ranked::eloDeltaWithK로 계산해 표로 보여 주고, 연승 보너스와 맵 진행도 감점 완화를 설명한다.
//
// 상대와의 레이팅 차이 열(-400 ~ +400), 팝업 크기, 표의 첫 열/오른쪽 끝 x 좌표.
// m_win이 true면 WIN 표, false면 LOSS 표를 그린다.
class RankedPointsPopup final : public ::versus::DeferredPopup {
    static constexpr std::array<int, 7> kDiffs = {-400, -200, -100, 0, 100, 200, 400};
    static constexpr float kWidth = 440.f, kHeight = 292.f;
    static constexpr float kFirstColumn = 122.f, kRight = 422.f;
    CCNode* m_table = nullptr;
    CCMenuItemSpriteExtra *m_winTab = nullptr, *m_lossTab = nullptr;
    CCNode *m_streakPanel = nullptr, *m_mapPanel = nullptr;
    bool m_win = true;

    // K값 구간 행의 이름("Below 1200", "1200-1499", "2400+" 등).
    static std::string bandName(size_t index) {
        auto const& bands = ranked::ELO_K_BANDS;
        if (index == 0) return fmt::format("Below {:.0f}", bands[0].below);
        if (index + 1 == bands.size()) return fmt::format("{:.0f}+", bands[index - 1].below);
        return fmt::format("{:.0f}-{:.0f}", bands[index - 1].below, bands[index].below - 1.);
    }
    // 차이 열의 가운데 x 좌표.
    static float columnX(size_t column) {
        float const pitch = (kRight - kFirstColumn) / kDiffs.size();
        return kFirstColumn + pitch * (column + .5f);
    }
    // 내 K값이 속한 행 번호. 랭크 정보가 없으면 -1.
    static int ownRow() {
        auto const* state = Service::get().rankedState();
        if (!state) return -1;
        int const k = ranked::eloK(state->profile.matches, state->profile.rating);
        for (size_t i = 0; i < ranked::ELO_K_BANDS.size(); ++i)
            if (ranked::ELO_K_BANDS[i].k == k) return static_cast<int>(i);
        return -1;
    }

    bool init() override {
        if (!Popup::init(kWidth, kHeight, "GJ_square02.png")) return false;
        setTitle("RATING POINTS", "bigFont.fnt", .6f);
        m_winTab = button(m_buttonMenu, this, menu_selector(RankedPointsPopup::onWin), "WIN",
                          {kWidth / 2.f - 38.f, 248.f}, .5f);
        m_lossTab = button(m_buttonMenu, this, menu_selector(RankedPointsPopup::onLoss), "LOSS",
                           {kWidth / 2.f + 38.f, 248.f}, .5f, "GJ_button_06.png");
        m_winTab->setID("rating-points-win"_spr);
        m_lossTab->setID("rating-points-loss"_spr);
        m_table = CCNode::create();
        m_mainLayer->addChild(m_table, 2);

        // WIN 탭 하단: 연승 보너스 안내 패널.
        auto* streak = panel(m_mainLayer, {14.f, 47.f}, {kWidth - 28.f, 26.f}, false);
        m_streakPanel = streak;
        label(streak, "WIN STREAK BONUS", {10.f, 13.f}, .3f, 118.f, ccc3(255, 205, 126), true);
        std::string chips;
        for (int wins = 2; wins <= 5; ++wins) {
            if (!chips.empty()) chips += "     ";
            chips += fmt::format("{}{} in a row +{}", wins, wins == 5 ? "+" : "", ranked::streakBonus(wins));
        }
        label(streak, chips, {(kWidth - 28.f + 132.f) / 2.f, 13.f}, .5f, kWidth - 28.f - 140.f, ccc3(130, 255, 165),
              false, "chatFont.fnt");
        auto const* own = Service::get().rankedState();
        std::string const tier = own ? own->profile.tier : std::string();
        int const full = ranked::mapFullGap(tier);
        // LOSS 탭 하단: 맵 진행도 차이에 따른 패배 감점 완화 안내 패널(내 티어 기준 gap).
        auto* map = panel(m_mainLayer, {14.f, 47.f}, {kWidth - 28.f, 26.f}, false);
        m_mapPanel = map;
        label(map, "MAP EXPERIENCE", {10.f, 18.f}, .3f, 118.f, ccc3(120, 228, 255), true);
        label(map, full < 100 ? fmt::format("{}: smaller gaps count", tier) : std::string("lower best % loses less"),
              {10.f, 7.f}, .32f, 118.f, kMuted, true, "chatFont.fnt");
        std::string gaps;
        for (int quarter = 0; quarter <= 4; ++quarter) {
            int const gap = (full * quarter + 2) / 4;
            if (!gaps.empty()) gaps += "    ";
            gaps += fmt::format("gap {}%{}: -{:.0f}%", gap, quarter == 4 && full < 100 ? "+" : "",
                                ranked::mapLossReduction(0, gap, tier) * 100.);
        }
        label(map, gaps, {(kWidth - 28.f + 132.f) / 2.f, 13.f}, .46f, kWidth - 28.f - 140.f, ccc3(120, 228, 255), false,
              "chatFont.fnt");
        // 공통 안내 문구: 신규 유저 최소 K, 무승부, 티어와 레이팅의 관계, 듀얼 제외 규칙.
        label(m_mainLayer,
              fmt::format("First {} Ranked matches: never less than the {} row.  Draw: no change, new map.",
                          ranked::ELO_NEW_PLAYER_MATCHES, bandName(1)),
              {kWidth / 2.f, 33.f}, .42f, kWidth - 30.f, kIce, false, "chatFont.fnt");
        label(m_mainLayer,
              "Points follow your rating, not your tier (tiers are ladder percentiles). Duels: no rating, no streak.",
              {kWidth / 2.f, 19.f}, .42f, kWidth - 30.f, kMuted, false, "chatFont.fnt");
        render();
        return true;
    }
    // 선택된 탭에 맞게 표를 다시 그린다. 각 칸은 레이팅 0 기준 상대 차이로 계산한 변동량이고,
    // 내 K값 행은 강조한다.
    void render() {
        m_table->removeAllChildrenWithCleanup(true);
        auto tab = [](CCMenuItemSpriteExtra* item, bool selected) {
            cascadeOpacity(item);
            item->setOpacity(selected ? 255 : 110);
        };
        tab(m_winTab, m_win);
        tab(m_lossTab, !m_win);
        if (m_streakPanel) m_streakPanel->setVisible(m_win);
        if (m_mapPanel) m_mapPanel->setVisible(!m_win);
        ccColor3B const good = ccc3(130, 255, 165), bad = ccc3(255, 160, 160);
        label(m_table, m_win ? "Points for a WIN" : "Points for a LOSS", {(14.f + kFirstColumn) / 2.f, 226.f}, .3f,
              104.f, m_win ? good : bad);
        label(m_table, "OPPONENT'S RATING vs YOURS", {(kFirstColumn + kRight) / 2.f, 226.f}, .28f,
              kRight - kFirstColumn, kIce);
        label(m_table, "YOUR RATING", {(14.f + kFirstColumn) / 2.f, 210.f}, .3f, 104.f, kIce);
        for (size_t column = 0; column < kDiffs.size(); ++column) {
            int const diff = kDiffs[column];
            auto text = diff == 0 ? std::string("EVEN") : fmt::format("{:+}", diff);
            label(m_table, text, {columnX(column), 210.f}, .32f, 40.f, diff < 0 ? kMuted : ccWHITE);
        }
        int const mine = ownRow();
        auto const& bands = ranked::ELO_K_BANDS;
        for (size_t row = 0; row < bands.size(); ++row) {
            float const y = 192.f - 19.f * row;
            bool const own = static_cast<int>(row) == mine;
            auto* bg = panel(m_table, {14.f, y - 9.5f}, {kRight - 14.f, 19.f}, false);
            if (own)
                if (auto* slice = bg->getChildByType<NineSlice*>(0)) {
                    slice->setColor(ccc3(40, 110, 70));
                    slice->setOpacity(220);
                }
            label(m_table, bandName(row), {(14.f + kFirstColumn) / 2.f + (own ? 8.f : 0.f), y}, .5f, 92.f,
                  own ? ccc3(255, 230, 120) : ccWHITE, false, "chatFont.fnt");
            if (own) label(m_table, "YOU", {20.f, y}, .22f, 26.f, ccc3(255, 230, 120), true);
            for (size_t column = 0; column < kDiffs.size(); ++column) {
                int const delta = ranked::eloDeltaWithK(bands[row].k, 0., kDiffs[column], m_win ? 1. : 0.);
                label(m_table, fmt::format("{:+}", delta), {columnX(column), y}, .56f, 40.f, m_win ? good : bad, false,
                      "chatFont.fnt");
            }
        }
        // 하단 요약: 내 레이팅 기준으로 동급 상대와의 다음 승리/패배 변동량(연승 보너스, 맵 완화 포함).
        if (auto const* state = Service::get().rankedState(); state && mine >= 0) {
            auto const& profile = state->profile;
            int const even = ranked::eloDelta(profile.rating, profile.rating, m_win ? 1. : 0., profile.matches);
            auto text =
                m_win ? fmt::format("You: R{:.0f}, {} win{} in a row. Next win vs an equal opponent: {:+}{}",
                                    profile.rating, profile.winStreak, profile.winStreak == 1 ? "" : "s", even,
                                    ranked::streakBonus(profile.winStreak + 1) > 0
                                        ? fmt::format(" +{} streak bonus", ranked::streakBonus(profile.winStreak + 1))
                                        : std::string())
                      : fmt::format(
                            "You: R{:.0f}. Loss vs an equal opponent: {:+} ({:+} if your map best was {}%{} lower). "
                            "Vs a lower-rated one: a bit more.",
                            profile.rating, even,
                            ranked::reducedLoss(even, ranked::mapLossReduction(0, 100, profile.tier)),
                            ranked::mapFullGap(profile.tier), ranked::mapFullGap(profile.tier) < 100 ? "+" : "");
            label(m_table, text, {kWidth / 2.f, 80.f}, .44f, kWidth - 34.f, ccc3(255, 230, 120), false, "chatFont.fnt");
        }
    }
    void onWin(CCObject*) {
        if (!m_win) {
            m_win = true;
            render();
        }
    }
    void onLoss(CCObject*) {
        if (m_win) {
            m_win = false;
            render();
        }
    }

public:
    CREATE_FUNC(RankedPointsPopup);
};
