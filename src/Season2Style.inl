// Season 2 화면 공통 스타일 (VersusUI.cpp에 #include되는 조각): 갈색 계열 배경 / 카드 색과 불투명도,
// 순위 글꼴, 문자열 대문자 변환 도우미.
namespace s2 {
constexpr ccColor3B kBase{38, 20, 10};
constexpr ccColor3B kTiles{108, 57, 25};
constexpr GLubyte kTileOpacity = 145;
constexpr ccColor3B kCard{112, 55, 22};
constexpr GLubyte kCardOpacity = 220;
constexpr ccColor3B kFrame = kCard;
constexpr GLubyte kFrameOpacity = kCardOpacity;
constexpr ccColor3B kSoft{255, 214, 170};
constexpr char const* kRankFont = "gjFont48.fnt";
inline std::string upper(std::string text) {
    for (auto& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}
}
