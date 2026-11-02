// 파티클 시스템 생성 헬퍼. Versus가 만드는 모든 CCParticleSystemQuad는 이 함수들을 거친다.
// 특정 환경(JIT 없는 iOS + QOLMod)에서는 파티클 생성을 막아 nullptr을 돌려준다.
#pragma once
#include <Geode/Geode.hpp>

namespace versus {

// 이 환경에서 Versus 자체 파티클을 써도 되는지 여부. JIT 없는 iOS에서 QOLMod가 로드되어
// 있으면 false(문제 소지가 있어 끔)이고, 결과는 첫 호출 때 한 번만 계산해 캐시한다.
inline bool particlesAllowed() {
#ifdef GEODE_IS_IOS
    static bool const allowed = [] {
        auto* loader = geode::Loader::get();
        bool const risky = loader->isPatchless() && loader->isModLoaded("thesillydoggo.qolmod");
        if (risky) geode::log::info("Versus: JIT-less iOS with QOLMod - Versus shows no particle effects of its own");
        return !risky;
    }();
    return allowed;
#else
    return true;
#endif
}

// plist 파일로 파티클 시스템을 만든다. 파티클이 막혀 있으면 nullptr (호출부에서 null 체크 필요).
inline cocos2d::CCParticleSystemQuad* particleSystem(char const* plist) {
    return particlesAllowed() ? cocos2d::CCParticleSystemQuad::create(plist, false) : nullptr;
}

// 최대 파티클 수만 지정해 빈 파티클 시스템을 만든다. 막혀 있으면 nullptr.
inline cocos2d::CCParticleSystemQuad* particleSystem(unsigned total) {
    return particlesAllowed() ? cocos2d::CCParticleSystemQuad::createWithTotalParticles(total, false) : nullptr;
}

}
