// PlayLayer 대상 무결성(외부 치트) 감시의 공개 인터페이스. 구현은 IntegrityMonitor.cpp.
#pragma once
class PlayLayer;
namespace versus::integrity {
// 현재 프레임이 drawScene 안에서 그려지는 중이면 true (해당 PlayLayer일 때만).
bool framePending(PlayLayer* layer);
// 250ms 제한을 무시하고 외부 치트 검사를 즉시 한 번 실행한다.
void checkNow(PlayLayer* layer);
}
