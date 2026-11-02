// 플랫폼별 설치기 인터페이스. 구현은 UpdateInstaller.cpp.
#pragma once
#include "UpdateCore.hpp"
#include <memory>

namespace versus::update {
// 설치에 필요한 경로들(작업 폴더, 받은 패키지, mods 폴더)과 매니페스트.
struct InstallContext {
    fs::path directory, package, modsDirectory;
    Manifest manifest;
};
// 설치기: automatic()은 앱 안에서 자동 설치가 되는지,
// prepare는 패키지를 설치 위치에 준비하고, restart는 게임을 재시작한다.
class UpdateInstaller {
public:
    virtual ~UpdateInstaller() = default;
    virtual bool automatic() const = 0;
    virtual void prepare(InstallContext const& context) = 0;
    virtual void restart() = 0;
};
// 현재 플랫폼의 설치기를 만든다.
std::unique_ptr<UpdateInstaller> platformInstaller();
// 이 플랫폼에서 게임을 재시작할 수 있는지.
bool canRestart();
// 업데이트 후 첫 부팅이 정상적으로 끝났음을 기록한다.
void confirmUpdateBoot();
}
