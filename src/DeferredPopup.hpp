// 뒤로가기 키 처리를 다음 메인 스레드 틱으로 미루는 Popup 기반 클래스.
#pragma once
#include "KeyDefer.hpp"

#include <Geode/ui/Popup.hpp>

namespace versus {
// 뒤로가기가 연속으로 들어와도 한 번만 처리하도록 m_backQueued로 막는다.
class DeferredPopup : public geode::Popup {
    bool m_backQueued = false;

public:
    void keyBackClicked() override {
        if (m_backQueued) return;
        m_backQueued = true;
        deferBack(this, [this] {
            m_backQueued = false;
            geode::Popup::keyBackClicked();
        });
    }
    // 코드에서 팝업을 닫는다. 이미 닫았거나 부모가 없으면 무시한다.
    void closeFromCode() {
        if (m_closedFromCode || !getParent()) return;
        m_closedFromCode = true;
        onClose(nullptr);
    }

private:
    bool m_closedFromCode = false;
};
}
