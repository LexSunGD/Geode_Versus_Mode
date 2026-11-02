// 메뉴 커서가 필요한 구간을 세는 RAII 범위 객체.
#pragma once

#include <cstddef>

namespace versus::cursor {
// 현재 활성인 Scope 개수. 0이 아니면 커서를 보여야 한다.
inline std::size_t activeScopes = 0;

// enter/leave로 activeScopes를 한 번씩만 증감한다(중복 호출 안전). 소멸 시 자동 leave.
class Scope final {
    bool m_active = false;

public:
    Scope() = default;
    Scope(Scope const&) = delete;
    Scope& operator=(Scope const&) = delete;
    ~Scope() { leave(); }
    void enter() {
        if (!m_active) {
            m_active = true;
            ++activeScopes;
        }
    }
    void leave() {
        if (m_active) {
            m_active = false;
            --activeScopes;
        }
    }
    static bool required() { return activeScopes != 0; }
};
}
