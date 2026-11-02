// 지뢰찾기 미니게임의 보드 로직(UI와 분리). 8x8 칸에 지뢰 10개.
// 지뢰는 첫 번째로 연 칸과 그 주변 칸을 피해 seed 기반으로 배치한다.
#pragma once
#include <algorithm>
#include <array>
#include <numeric>
#include <random>
#include <vector>

namespace versus::mines {
// 게임 보드. 상태는 Ready(첫 클릭 전) -> Playing -> Won/Lost.
class Board {
public:
    static constexpr int SIDE = 8, COUNT = SIDE * SIDE, MINES = 10;
    // 칸 하나: 지뢰 여부, 열림, 깃발, 주변 지뢰 수.
    struct Cell {
        bool mine = false, open = false, flag = false;
        int adjacent = 0;
    };
    enum class State { Ready, Playing, Won, Lost };

private:
    std::array<Cell, COUNT> m_cells{};
    State m_state = State::Ready;
    unsigned m_seed;
    int m_flags = 0, m_opened = 0;
    // at 칸의 인접 칸(최대 8개)마다 fn을 호출한다.
    template <class Fn> void neighbors(int at, Fn fn) const {
        for (int y = std::max(0, at / SIDE - 1); y <= std::min(SIDE - 1, at / SIDE + 1); ++y)
            for (int x = std::max(0, at % SIDE - 1); x <= std::min(SIDE - 1, at % SIDE + 1); ++x)
                if (y * SIDE + x != at) fn(y * SIDE + x);
    }
    // 첫 클릭 칸과 주변 3x3을 제외한 칸 중에서 seed로 섞어 지뢰를 놓고, 주변 지뢰 수를
    // 계산한다.
    void place(int first) {
        std::vector<int> choices;
        for (int i = 0; i < COUNT; ++i)
            if (std::abs(i / SIDE - first / SIDE) > 1 || std::abs(i % SIDE - first % SIDE) > 1) choices.push_back(i);
        std::mt19937 random(m_seed);
        std::shuffle(choices.begin(), choices.end(), random);
        for (int i = 0; i < MINES; ++i) m_cells[choices[i]].mine = true;
        for (int i = 0; i < COUNT; ++i) neighbors(i, [&](int n) { m_cells[i].adjacent += m_cells[n].mine; });
        m_state = State::Playing;
    }

public:
    explicit Board(unsigned seed = 0) : m_seed(seed) {}
    State state() const { return m_state; }
    int flags() const { return m_flags; }
    int opened() const { return m_opened; }
    Cell const& cell(int i) const { return m_cells.at(i); }
    bool finished() const { return m_state == State::Won || m_state == State::Lost; }
    // 깃발을 토글한다. 이미 열렸거나 끝난 게임, 깃발이 지뢰 수만큼 꽂힌 상태에서는 새로 꽂지
    // 않는다.
    void flag(int i) {
        if (i < 0 || i >= COUNT || finished() || m_cells[i].open) return;
        auto& c = m_cells[i];
        if (!c.flag && m_flags >= MINES) return;
        c.flag = !c.flag;
        m_flags += c.flag ? 1 : -1;
    }
    // 칸을 연다. 첫 클릭이면 지뢰를 배치하고, 지뢰를 열면 패배.
    // 주변 지뢰가 0이면 인접 칸을 반복해서 연다. 지뢰가 아닌 칸을 모두 열면 승리.
    void reveal(int i) {
        if (i < 0 || i >= COUNT || finished() || m_cells[i].open || m_cells[i].flag) return;
        if (m_state == State::Ready) place(i);
        if (m_cells[i].mine) {
            m_cells[i].open = true;
            m_state = State::Lost;
            return;
        }
        std::vector<int> pending{i};
        while (!pending.empty()) {
            int at = pending.back();
            pending.pop_back();
            auto& c = m_cells[at];
            if (c.open || c.flag || c.mine) continue;
            c.open = true;
            ++m_opened;
            if (!c.adjacent)
                neighbors(at, [&](int n) {
                    if (!m_cells[n].open) pending.push_back(n);
                });
        }
        if (m_opened == COUNT - MINES) m_state = State::Won;
    }
};
}
