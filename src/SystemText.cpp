// 시스템 폰트로 텍스트를 그리는 구현(BMFont에 없는 문자용).
// macOS/iOS는 CoreText로 직접 비트맵을 그려 텍스처로 만들고, 그 외 플랫폼은 CCLabelTTF를
// 쓴다.
#include "SystemText.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#if defined(GEODE_IS_MACOS) || defined(GEODE_IS_IOS)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#define VERSUS_CORE_TEXT 1
#else
#define VERSUS_CORE_TEXT 0
#endif

using namespace geode::prelude;

namespace versus {

// 플랫폼별 시스템 폰트 이름 (Windows: Malgun Gothic, Apple: AppleSDGothicNeo, 그 외:
// sans-serif).
char const* systemFontName(TextWeight weight) {
#ifdef GEODE_IS_WINDOWS
    (void)weight;
    return "Malgun Gothic";
#elif defined(GEODE_IS_MACOS) || defined(GEODE_IS_IOS)
    return weight == TextWeight::Bold ? "AppleSDGothicNeo-Bold" : "AppleSDGothicNeo-Regular";
#else
    return weight == TextWeight::Bold ? "sans-serif-medium" : "sans-serif";
#endif
}

// CoreText 경로. CF 객체를 자동 해제하는 래퍼와 비트맵 렌더러.
#if VERSUS_CORE_TEXT
namespace {
template <typename T> struct CF {
    T value = nullptr;
    explicit CF(T held) : value(held) {}
    CF(CF const&) = delete;
    CF& operator=(CF const&) = delete;
    ~CF() {
        if (value) CFRelease(value);
    }
    explicit operator bool() const { return value != nullptr; }
};

// RGBA 픽셀 버퍼.
struct Bitmap {
    std::vector<unsigned char> pixels;
    int width = 0, height = 0;
};

// 비트맵 한 변의 최대 픽셀 수.
constexpr int kMaxPixels = 4096;

// 텍스트를 흰색 RGBA 비트맵으로 그린다. width가 0이면 줄바꿈 없이 글자 폭에 맞추고,
// 아니면 그 폭에서 단어 단위로 줄바꿈한다. scale은 화면 배율.
Bitmap render(std::string const& text, float size, float width, CCTextAlignment alignment, TextWeight weight,
              float scale) {
    Bitmap out;
    CF<CFStringRef> string(CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<UInt8 const*>(text.data()),
                                                   static_cast<CFIndex>(text.size()), kCFStringEncodingUTF8, false));
    CF<CFStringRef> empty(CFStringCreateWithCString(kCFAllocatorDefault, "", kCFStringEncodingUTF8));
    CF<CFStringRef> name(CFStringCreateWithCString(kCFAllocatorDefault, systemFontName(weight), kCFStringEncodingUTF8));
    CF<CTFontRef> font(CTFontCreateWithName(name.value, size * scale, nullptr));
    CF<CGColorSpaceRef> space(CGColorSpaceCreateDeviceRGB());
    CGFloat const white[4] = {1.0, 1.0, 1.0, 1.0};
    CF<CGColorRef> color(CGColorCreate(space.value, white));
    if (!font || !space || !color) return out;
    CTTextAlignment align = alignment == kCCTextAlignmentCenter  ? kCTTextAlignmentCenter
                            : alignment == kCCTextAlignmentRight ? kCTTextAlignmentRight
                                                                 : kCTTextAlignmentLeft;
    CTLineBreakMode lineBreak = kCTLineBreakByWordWrapping;
    CTParagraphStyleSetting const settings[2] = {
        {kCTParagraphStyleSpecifierAlignment, sizeof(align), &align},
        {kCTParagraphStyleSpecifierLineBreakMode, sizeof(lineBreak), &lineBreak},
    };
    CF<CTParagraphStyleRef> paragraph(CTParagraphStyleCreate(settings, 2));
    void const* keys[3] = {kCTFontAttributeName, kCTForegroundColorAttributeName, kCTParagraphStyleAttributeName};
    void const* values[3] = {font.value, color.value, paragraph.value};
    CF<CFDictionaryRef> attributes(CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CF<CFAttributedStringRef> attributed(
        CFAttributedStringCreate(kCFAllocatorDefault, string ? string.value : empty.value, attributes.value));
    if (!attributed) return out;
    CF<CTFramesetterRef> setter(CTFramesetterCreateWithAttributedString(attributed.value));
    if (!setter) return out;
    CGFloat const limit = width > 0.f ? std::min<CGFloat>(width * scale, kMaxPixels) : CGFLOAT_MAX;
    CFRange fit{};
    CGSize const needed = CTFramesetterSuggestFrameSizeWithConstraints(setter.value, CFRangeMake(0, 0), nullptr,
                                                                       CGSizeMake(limit, CGFLOAT_MAX), &fit);
    out.width = std::clamp(static_cast<int>(std::ceil(width > 0.f ? limit : needed.width)), 1, kMaxPixels);
    out.height = std::clamp(static_cast<int>(std::ceil(needed.height)) + 2, 1, kMaxPixels);
    out.pixels.assign(static_cast<size_t>(out.width) * out.height * 4, 0);
    CF<CGContextRef> context(CGBitmapContextCreate(
        out.pixels.data(), out.width, out.height, 8, out.width * 4, space.value,
        static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
    if (!context) {
        out = {};
        return out;
    }
    CGContextSetTextMatrix(context.value, CGAffineTransformIdentity);
    CF<CGMutablePathRef> path(CGPathCreateMutable());
    CGPathAddRect(path.value, nullptr, CGRectMake(0, 0, out.width, out.height));
    CF<CTFrameRef> frame(CTFramesetterCreateFrame(setter.value, CFRangeMake(0, 0), path.value, nullptr));
    if (frame) CTFrameDraw(frame.value, context.value);
    // 색은 흰색으로 통일하고 알파만 남겨, 이후 setColor로 색을 입힐 수 있게 한다.
    for (size_t i = 0; i < out.pixels.size(); i += 4) {
        if (!out.pixels[i + 3]) continue;
        out.pixels[i] = out.pixels[i + 1] = out.pixels[i + 2] = 255;
    }
    return out;
}

// CoreText 비트맵 텍스처를 쓰는 라벨 스프라이트. 텍스트가 바뀌면 텍스처를 다시 만든다.
class CoreTextLabel final : public CCSprite {
    std::string m_text;
    float m_size = 12.f, m_width = 0.f;
    CCTextAlignment m_alignment = kCCTextAlignmentLeft;
    TextWeight m_weight = TextWeight::Regular;
    CCSize m_points;

    static float pixelScale() { return std::clamp(CCDirector::sharedDirector()->getContentScaleFactor(), 1.f, 3.f); }
    CCTexture2D* texture() {
        float const scale = pixelScale();
        auto bitmap = render(m_text, m_size, m_width, m_alignment, m_weight, scale);
        if (bitmap.pixels.empty()) return nullptr;
        auto* result = new CCTexture2D();
        if (!result->initWithData(bitmap.pixels.data(), kCCTexture2DPixelFormat_RGBA8888, bitmap.width, bitmap.height,
                                  CCSize(static_cast<float>(bitmap.width), static_cast<float>(bitmap.height)))) {
            result->release();
            return nullptr;
        }
        m_points = CCSize(bitmap.width / scale, bitmap.height / scale);
        result->autorelease();
        return result;
    }

protected:
    // 텍스처 좌표를 v축을 뒤집어 지정한다.
    void setTextureCoords(CCRect const&) override {
        m_sQuad.bl.texCoords.u = 0.f;
        m_sQuad.bl.texCoords.v = 1.f;
        m_sQuad.br.texCoords.u = 1.f;
        m_sQuad.br.texCoords.v = 1.f;
        m_sQuad.tl.texCoords.u = 0.f;
        m_sQuad.tl.texCoords.v = 0.f;
        m_sQuad.tr.texCoords.u = 1.f;
        m_sQuad.tr.texCoords.v = 0.f;
    }

public:
    static CoreTextLabel* create(std::string const& text, float size, float width, CCTextAlignment alignment,
                                 TextWeight weight) {
        auto* label = new CoreTextLabel();
        label->m_text = text;
        label->m_size = size;
        label->m_width = width;
        label->m_alignment = alignment;
        label->m_weight = weight;
        auto* texture = label->texture();
        if (!(texture ? label->initWithTexture(texture, CCRect(0.f, 0.f, label->m_points.width, label->m_points.height))
                      : label->init())) {
            delete label;
            return nullptr;
        }
        label->autorelease();
        return label;
    }
    void setText(std::string const& text) {
        if (text == m_text) return;
        m_text = text;
        if (auto* texture = this->texture()) {
            setTexture(texture);
            setTextureRect(CCRect(0.f, 0.f, m_points.width, m_points.height));
        } else
            setTextureRect(CCRect(0.f, 0.f, 0.f, 0.f));
    }
};
}
#endif

// CoreText 라벨을 우선 시도하고, 안 되면 CCLabelTTF로 만든다.
// CoreText를 쓰지 않는 플랫폼의 Bold는 약간 옮긴 그림자 라벨을 덧그려 굵게 보이게 한다.
CCSprite* createSystemText(std::string const& text, float size, float width, CCTextAlignment alignment,
                           TextWeight weight) {
#if VERSUS_CORE_TEXT
    if (auto* label = CoreTextLabel::create(text, size, width, alignment, weight)) return label;
#endif
    auto* label = CCLabelTTF::create(text.c_str(), systemFontName(weight), size, {width, 0.f}, alignment);
    if (label && weight == TextWeight::Bold && !VERSUS_CORE_TEXT) {
        auto* shadow = CCLabelTTF::create(text.c_str(), systemFontName(weight), size, {width, 0.f}, alignment);
        if (shadow) {
            shadow->setTag(0x5b01d);
            shadow->setAnchorPoint({0.f, 0.f});
            shadow->setPosition({.55f, 0.f});
            label->setCascadeColorEnabled(true);
            label->setCascadeOpacityEnabled(true);
            label->addChild(shadow);
        }
    }
    return label;
}

// createSystemText로 만든 라벨의 텍스트를 바꾼다. 그림자 라벨이 있으면 같이 바꾼다.
void setSystemText(CCSprite* label, std::string const& text) {
    if (!label) return;
#if VERSUS_CORE_TEXT
    if (auto* coreText = typeinfo_cast<CoreTextLabel*>(label)) {
        coreText->setText(text);
        return;
    }
#endif
    if (auto* ttf = typeinfo_cast<CCLabelTTF*>(label)) {
        ttf->setString(text.c_str());
        if (auto* shadow = typeinfo_cast<CCLabelTTF*>(ttf->getChildByTag(0x5b01d))) shadow->setString(text.c_str());
    }
}

}
