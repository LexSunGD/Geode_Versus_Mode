// 에디터 Plaza Test 흐름의 상태 조회 / 전환 인터페이스 (구현은 EditorPlaza.cpp).
// waiting: Versus 진입 대기 중, entered: Season 2 진입, homeReached: 메인 화면 도달,
// cancel: 취소, backToLevel: 테스트 대상 레벨 페이지로 복귀 (복귀했으면 true).
#pragma once

namespace versus::editorplaza {
bool waiting();
void entered();
void homeReached();
void cancel();
bool backToLevel();
}
