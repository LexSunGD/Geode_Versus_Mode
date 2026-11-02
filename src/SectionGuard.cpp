// 오브젝트 위치가 NaN이면 GD가 section에 넣다가 크래시하므로, addToSection 직전에
// 위치를 복구하는 GJBaseGameLayer 훅. iOS에서는 컴파일하지 않는다.
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

#include <cmath>

using namespace geode::prelude;

#ifndef GEODE_IS_IOS
namespace {
// 복구 로그를 남긴 횟수. 로그 폭주를 막기 위해 5번까지만 기록한다.
int s_repairsLogged = 0;
// NaN인 좌표만 오브젝트의 시작 위치로 되돌린다. 시작 위치도 유효하지 않으면 0으로 둔다.
void repairPosition(GameObject* object) {
    auto const start = object->m_startPosition;
    bool const badX = std::isnan(object->m_positionX), badY = std::isnan(object->m_positionY);
    double const x = badX ? (std::isfinite(start.x) ? start.x : 0.) : object->m_positionX;
    double const y = badY ? (std::isfinite(start.y) ? start.y : 0.) : object->m_positionY;
    if (s_repairsLogged < 5) {
        ++s_repairsLogged;
        log::warn("Versus: object {} had no real position (NaN) - put back at ({}, {}) before GD files it (a crash)",
                  object->m_objectID, x, y);
    }
    object->m_positionX = x;
    object->m_positionY = y;
}
}

class $modify(VersusSectionGuard, GJBaseGameLayer){
    // x 또는 y가 NaN이면 복구한 뒤 원래 addToSection을 호출한다.
    void addToSection(GameObject * object){
        if (object && (std::isnan(object->m_positionX) || std::isnan(object->m_positionY))) repairPosition(object);
GJBaseGameLayer::addToSection(object);
}
}
;
#endif
