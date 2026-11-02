// cocos2d 노드를 소유하지 않고 약하게 가리키는 Weak<T> 포인터.
// 노드에 Lifeline 객체를 달아 두고, 노드가 해제되면 Lifeline 소멸자가 플래그를 꺼서
// 비동기 콜백이 해제된 노드를 건드리지 않게 한다.
#pragma once
#include <Geode/utils/casts.hpp>
#include <Geode/utils/cocos.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace versus {

// 노드의 userObject로 붙는 생존 표지. 소멸할 때 공유 플래그(alive)를 false로 만든다.
class Lifeline final : public cocos2d::CCObject {
public:
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    ~Lifeline() override { *alive = false; }

    static constexpr std::string_view kKey = "tipp7.versus/lifeline";

    // 노드에 붙은 Lifeline의 alive 플래그를 약한 참조로 돌려준다. 없으면 만들어 붙인다.
    static std::weak_ptr<bool> of(cocos2d::CCNode* node) {
        if (!node) return {};
        if (auto* existing = geode::cast::typeinfo_cast<Lifeline*>(node->getUserObject(kKey))) return existing->alive;
        auto* line = new Lifeline();
        line->autorelease();
        node->setUserObject(std::string(kKey), line);
        return line->alive;
    }
};

// 노드 포인터와 생존 플래그를 함께 들고 있는 약한 참조.
template <class T> class Weak {
    T* m_node = nullptr;
    std::weak_ptr<bool> m_alive;

public:
    Weak() = default;
    Weak(std::nullptr_t) {}
    Weak(T* node) { *this = node; }

    Weak& operator=(T* node) {
        m_node = node;
        m_alive = node ? Lifeline::of(node) : std::weak_ptr<bool>{};
        return *this;
    }
    Weak& operator=(std::nullptr_t) {
        m_node = nullptr;
        m_alive.reset();
        return *this;
    }

    // 노드가 아직 살아 있으면 Ref로 잡아 돌려주고, 아니면 빈 Ref를 돌려준다.
    geode::Ref<T> lock() const {
        auto const alive = m_alive.lock();
        if (!m_node || !alive || !*alive || m_node->retainCount() == 0) return geode::Ref<T>();
        return geode::Ref<T>(m_node);
    }
    // 살아 있는지 여부. peek()는 생존 확인 없이 원시 포인터만 돌려준다.
    bool valid() const { return lock() != nullptr; }
    T* peek() const { return m_node; }
};

// ref를 비운 뒤 value가 있으면 T*로 변환해 다시 가리킨다.
template <class T, class U> void rebind(Weak<T>& ref, U* value) {
    ref = nullptr;
    if (value) ref = static_cast<T*>(value);
}
}

using versus::Weak;
