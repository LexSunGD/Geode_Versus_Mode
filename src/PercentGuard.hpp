// Percentage Toggle 모드가 레벨의 측정 길이(m_timestamp)를 바꿔도
// 배틀 맵의 퍼센트 계산이 GD 2.2 기준으로 유지되게 하는 보호 기능의 선언.
#pragma once
class GJGameLevel;
class PlayLayer;

namespace versus::percent {
// Percentage Toggle 모드의 ID.
inline constexpr char const* kPercentageToggle = "zsa.percentage-toggle";
// 배틀용 레벨의 측정 길이를 기록해 둔다(필요하면 복구).
void prepareLevel(GJGameLevel* level);
// PlayLayer의 레벨 길이가 바뀌었으면 기록해 둔 값으로 되돌린다.
void enforce(PlayLayer* layer);
// Percentage Toggle이 이 레벨에 적용될 상황인지 알려 준다.
bool toggleWouldApply(GJGameLevel* level);
}
