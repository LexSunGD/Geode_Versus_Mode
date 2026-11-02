// "G.O.A.T" 프로필 이미지(얼굴)와 그 뒤에서 타오르는 불꽃 셰이더를 만든다.
// face()는 얼굴 + 둥근 불꽃, band()는 티어 이름 뒤에 까는 가로 불꽃 띠를 반환한다.
// 셰이더를 쓸 수 없는 환경에서는 불꽃 없이 동작한다.
#include "GoatFace.hpp"
#include "SpriteFrames.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

using namespace geode::prelude;

namespace versus::goat {
namespace {
// 얼굴 이미지 파일명.
// kDisc: 이미지 안에서 원판이 차지하는 비율, kInner: 불꽃이 시작되는 안쪽 반지름 비율.
constexpr char const* kImage = "s2-goat.png";
constexpr float kDisc = 246.f / 256.f;
constexpr float kInner = .58f;

// 정점 셰이더: 위치 변환 후 색과 텍스처 좌표를 그대로 넘긴다.
constexpr char const* kVertex = R"(
attribute vec4 a_position;
attribute vec2 a_texCoord;
attribute vec4 a_color;
#ifdef GL_ES
varying lowp vec4 v_fragmentColor;
varying mediump vec2 v_texCoord;
#else
varying vec4 v_fragmentColor;
varying vec2 v_texCoord;
#endif
void main() {
    gl_Position = CC_MVPMatrix * a_position;
    v_fragmentColor = a_color;
    v_texCoord = a_texCoord;
}
)";
// 불꽃 프래그먼트 셰이더 (fbm 노이즈로 위로 흐르는 불꽃을 그림).
// u_band.x > 0 이면 가로 띠 모드(x=가로세로비, y=세기), 아니면 얼굴 둘레의 원형 불꽃.
// 결과는 premultiplied alpha로 출력한다.
constexpr char const* kFragment = R"(
#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
varying lowp vec4 v_fragmentColor;
varying mediump vec2 v_texCoord;
#else
varying vec4 v_fragmentColor;
varying vec2 v_texCoord;
#endif
uniform float u_time;
uniform float u_inner;
uniform vec2 u_band;
uniform vec3 u_tint;

float hash(vec2 p) {
    p = fract(p * vec2(0.1031, 0.1030));
    p += dot(p, p.yx + 33.33);
    return fract((p.x + p.y) * p.x);
}
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}
float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * noise(p);
        p = p * 2.02 + vec2(1.7, 9.2);
        a *= 0.5;
    }
    return v;
}
vec4 band(vec2 uv) {
    float aspect = u_band.x;
    float t = u_time;
    vec2 p = vec2(uv.x * aspect, uv.y);
    vec2 q = vec2(p.x * 4.0, p.y * 2.2 - t * 2.0);
    float w = fbm(q * 0.6 + vec2(t * 0.25, 0.0));
    float n = fbm(q + vec2(w * 1.2, 0.0));
    float n2 = fbm(vec2(p.x * 9.0 + 3.1, p.y * 5.0 - t * 3.4));
    float flame = (1.0 - max(uv.y - 0.22, 0.0) / 0.72) * 1.1 + (n - 0.5) * 1.7 + (n2 - 0.5) * 0.6;
    float ends = smoothstep(0.0, 0.45, min(uv.x, 1.0 - uv.x) * aspect);
    flame *= ends * smoothstep(0.0, 0.2, uv.y);
    float heat = pow(clamp(flame, 0.0, 1.0), 1.35);
    vec3 col = mix(u_tint * 0.3, u_tint, smoothstep(0.1, 0.45, heat));
    col = mix(col, mix(u_tint, vec3(1.0), 0.45), smoothstep(0.45, 0.75, heat));
    col = mix(col, mix(u_tint, vec3(1.0), 0.75), smoothstep(0.8, 1.0, heat));
    float a = smoothstep(0.04, 0.45, heat) * u_band.y * smoothstep(0.0, 0.08, 1.0 - uv.y);
    return vec4(col * a, a);
}
void main() {
    if (u_band.x > 0.0) {
        gl_FragColor = band(vec2(v_texCoord.x, 1.0 - v_texCoord.y)) * v_fragmentColor.a;
        return;
    }
    vec2 p = vec2(v_texCoord.x * 2.0 - 1.0, 1.0 - v_texCoord.y * 2.0);
    float r = length(p);
    float d = (r - u_inner) / (1.0 - u_inner);
    float up = p.y / max(r, 0.0001);
    float reach = 0.42 + 0.5 * smoothstep(-0.7, 1.0, up);
    float t = u_time;
    vec2 q = vec2(p.x * 4.0, p.y * 2.2 - t * 2.1);
    float w = fbm(q * 0.6 + vec2(t * 0.25, 0.0));
    float n = fbm(q + vec2(w * 1.2, 0.0));
    float n2 = fbm(vec2(p.x * 9.0 + 3.1, p.y * 5.0 - t * 3.6));
    float flame = (1.0 - d / reach) * 1.15 + (n - 0.5) * 1.6 + (n2 - 0.5) * 0.6;
    flame *= smoothstep(-0.05, 0.06, d + 0.06);
    float heat = pow(clamp(flame, 0.0, 1.0), 1.35);
    vec3 col = mix(vec3(0.45, 0.03, 0.0), vec3(1.0, 0.42, 0.04), smoothstep(0.1, 0.45, heat));
    col = mix(col, vec3(1.0, 0.82, 0.35), smoothstep(0.45, 0.75, heat));
    col = mix(col, vec3(1.0, 0.97, 0.85), smoothstep(0.78, 1.0, heat));
    float a = smoothstep(0.04, 0.45, heat) * 0.92;
    float g = exp(-max(d, 0.0) * 9.0) * 0.35;
    vec3 rgb = col * a + vec3(1.0, 0.38, 0.05) * (g * (1.0 - a));
    float alpha = a + g * (1.0 - a);
    float edge = 1.0 - smoothstep(0.9, 1.0, r);
    gl_FragColor = vec4(rgb * edge, alpha * edge) * v_fragmentColor.a;
}
)";

// 셰이더 uniform 위치 모음.
struct Uniforms {
    GLint time = -1, inner = -1, band = -1, tint = -1;
};
// 컴파일된 불꽃 프로그램과 uniform 위치. failed면 이 환경에서 셰이더가 지원되지 않는다는 뜻.
struct Program {
    Ref<CCGLProgram> program;
    Uniforms uniforms;
    bool failed = false;
};
Program& shared() {
    static auto* value = new Program;
    return *value;
}

// 불꽃 셰이더 프로그램을 가져온다. 없거나 GL 컨텍스트 재생성으로 무효해졌으면 다시 컴파일/링크한다.
// 한 번 실패하면 failed로 기록해 다시 시도하지 않고 nullptr를 반환한다.
CCGLProgram* fireProgram(Uniforms& uniforms) {
    auto& s = shared();
    if (s.program && s.program->getProgram() && glIsProgram(s.program->getProgram())) {
        uniforms = s.uniforms;
        return s.program;
    }
    if (s.failed) return nullptr;
    s.program = nullptr;
    auto* program = new CCGLProgram();
    bool ok = program->initWithVertexShaderByteArray(kVertex, kFragment);
    if (ok) {
        program->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
        program->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
        program->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
        ok = program->link() && program->getProgram() != 0;
    }
    if (ok) {
        GLint linked = GL_FALSE;
        glGetProgramiv(program->getProgram(), GL_LINK_STATUS, &linked);
        ok = linked == GL_TRUE;
    }
    if (!ok) {
        program->release();
        s.failed = true;
        log::warn("Versus: the G.O.A.T fire shader is not supported here (no fire behind the face)");
        return nullptr;
    }
    program->updateUniforms();
    s.uniforms.time = program->getUniformLocationForName("u_time");
    s.uniforms.inner = program->getUniformLocationForName("u_inner");
    s.uniforms.band = program->getUniformLocationForName("u_band");
    s.uniforms.tint = program->getUniformLocationForName("u_tint");
    s.program = program;
    program->release();
    uniforms = s.uniforms;
    return program;
}

// 불꽃을 그리는 스프라이트. 시간 값을 uniform으로 넘기며 draw()에서 셰이더를 직접 설정한다.
class Fire final : public CCSprite {
    float m_time = 0.f;
    float m_aspect = 0.f, m_strength = 0.f;
    ccColor3B m_tint = ccWHITE;

public:
    static Fire* create(CCTexture2D* texture) {
        auto* fire = new Fire;
        if (texture && fire->initFire(texture)) {
            fire->autorelease();
            return fire;
        }
        delete fire;
        return nullptr;
    }
    // 띠 모드 설정: 가로세로비, 불꽃 세기(0~1), 색조.
    void setBand(float aspect, float strength, ccColor3B tint) {
        m_aspect = std::max(.01f, aspect);
        m_strength = std::clamp(strength, 0.f, 1.f);
        m_tint = tint;
    }
    // 셰이더 프로그램이 준비되지 않으면 실패. 불꽃끼리 움직임이 겹치지 않도록 시작 시간을 객체마다 다르게 둔다.
    bool initFire(CCTexture2D* texture) {
        if (!CCSprite::initWithTexture(texture)) return false;
        Uniforms uniforms;
        auto* program = fireProgram(uniforms);
        if (!program) return false;
        setShaderProgram(program);
        setBlendFunc({GL_ONE, GL_ONE_MINUS_SRC_ALPHA});
        m_time = static_cast<float>(reinterpret_cast<std::uintptr_t>(this) % 997) * .37f;
        scheduleUpdate();
        return true;
    }
    // 애니메이션 시간 누적 (프레임 시간 상한 적용, 600초마다 되감아 정밀도 유지).
    void update(float dt) override {
        m_time += std::clamp(dt, 0.f, .1f);
        if (m_time > 600.f) m_time -= 600.f;
    }
    // 매 프레임 uniform을 갱신한 뒤 기본 스프라이트 그리기를 호출한다.
    void draw() override {
        Uniforms uniforms;
        auto* program = fireProgram(uniforms);
        if (!program) return;
        if (getShaderProgram() != program) setShaderProgram(program);
        program->use();
        program->setUniformLocationWith1f(uniforms.time, m_time);
        program->setUniformLocationWith1f(uniforms.inner, kInner);
        program->setUniformLocationWith2f(uniforms.band, m_aspect, m_strength);
        program->setUniformLocationWith3f(uniforms.tint, m_tint.r / 255.f, m_tint.g / 255.f, m_tint.b / 255.f);
        CCSprite::draw();
    }
};

// 얼굴 이미지의 텍스처를 가져온다 (이미지가 없거나 크기가 0이면 nullptr).
CCTexture2D* imageTexture() {
    auto const resource = std::string(Mod::get()->expandSpriteName(kImage));
    auto* image = fileSprite(resource.c_str());
    if (!image || image->getContentSize().width <= 0.f || image->getContentSize().height <= 0.f) return nullptr;
    return image->getTexture();
}
}

// look 문자열이 G.O.A.T (이전 이름 포함)인지 확인한다. '@' 뒤의 부가 정보는 무시.
bool isGoat(std::string_view look) {
    auto const base = look.substr(0, look.find('@'));
    return base == kName || base == kOldName;
}

// size 크기의 상자 안에 얼굴 이미지를 가운데에 맞춰 넣어 반환한다.
// fire가 true이고 셰이더가 가능하면 얼굴 뒤(z=-1)에 불꽃 스프라이트를 함께 붙인다.
CCSprite* face(CCSize size, bool fire) {
    auto const resource = std::string(Mod::get()->expandSpriteName(kImage));
    auto* image = fileSprite(resource.c_str());
    if (!image || image->getContentSize().width <= 0.f || image->getContentSize().height <= 0.f) return nullptr;
    auto* box = CCSprite::create();
    if (!box) return nullptr;
    size.width = std::max(1.f, size.width);
    size.height = std::max(1.f, size.height);
    box->setContentSize(size);
    box->setCascadeColorEnabled(true);
    box->setCascadeOpacityEnabled(true);
    box->setID("goat-face"_spr);
    float const across = std::min(size.width, size.height);
    image->setScale(across / std::max(image->getContentSize().width, image->getContentSize().height));
    image->setPosition(size / 2.f);
    box->addChild(image, 1);
    if (fire) {
        if (auto* flames = Fire::create(image->getTexture())) {
            flames->setID("goat-fire"_spr);
            float const disc = across * kDisc;
            flames->setScale(disc / kInner / std::max(1.f, flames->getContentSize().width));
            flames->setPosition(size / 2.f);
            box->addChild(flames, -1);
        }
    }
    return box;
}

// size 영역을 채우는 가로 불꽃 띠 스프라이트를 만든다 (티어 이름 뒤 장식용).
// strength는 불꽃 세기, tint는 색조. 셰이더를 쓸 수 없으면 nullptr.
CCSprite* band(CCSize size, ccColor3B tint, float strength) {
    if (size.width < 1.f || size.height < 1.f) return nullptr;
    auto* texture = imageTexture();
    auto* fire = texture ? Fire::create(texture) : nullptr;
    if (!fire) return nullptr;
    auto const own = fire->getContentSize();
    if (own.width <= 0.f || own.height <= 0.f) return nullptr;
    fire->setBand(size.width / size.height, strength, tint);
    fire->setScaleX(size.width / own.width);
    fire->setScaleY(size.height / own.height);
    fire->setID("tier-name-fire"_spr);
    return fire;
}
}
