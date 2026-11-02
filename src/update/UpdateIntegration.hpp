// 업데이트 모듈이 나머지 Versus(Service, UI)에 의존하는 부분을 모아 둔 인터페이스.
#pragma once
#include <functional>
#include <string>
namespace versus::update {
// 업데이트 다운로드용 인증 정보를 서버에서 받는다.
void authorizeUpdate(std::function<void(std::string, std::string)> callback);
// Versus 화면(방/랭크/홈)으로 이동한다.
void enterVersus();
// 대전이 진행 중인지.
bool matchInProgress();
// 아직 서버로 보내지 못한 기록이 있는지.
bool recordsPending();
// 운영진 계정인지 확인한다.
void checkStaffAccount(std::function<void(bool)> done);
}
