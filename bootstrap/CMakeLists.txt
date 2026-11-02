# 첫 설치용 부트스트랩 패키지: 모드 ID·저장 폴더는 같고 메타데이터만 최소화한다.
# 전체 패키지가 이 패키지를 교체하면 다음 재시작부터 전체 모드가 로드된다.
add_library(Versus SHARED ../src/main.cpp ../src/update/UpdateManager.cpp
    ../src/update/UpdateInstaller.cpp ../src/update/UpdateCheckScene.cpp ../src/SystemText.cpp Integration.cpp)
target_compile_definitions(Versus PRIVATE VERSUS_BOOTSTRAP=1)
if(APPLE)
    target_link_libraries(Versus "-framework CoreText" "-framework CoreGraphics" "-framework CoreFoundation")
endif()
if(MSVC)
    target_compile_options(Versus PRIVATE /Z7 /FS)
endif()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../logo.png" "${CMAKE_CURRENT_SOURCE_DIR}/logo.png" COPYONLY)
setup_geode_mod(Versus DONT_INSTALL)
