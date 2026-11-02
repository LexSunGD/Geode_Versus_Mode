// 멤버 전용 이모트 그림. 다른 .cpp 안에 #include 되어 쓰이는 파일로, 이미지 없이
// CCDrawNode(도형)와 라벨만으로 이모트를 코드로 그린다. 진입점은 memberEmoteArt().
namespace emote_art {
// 이모트 그림에 쓰는 공통 색: 외곽선, 얼굴, 흰색, 눈물, 입 안쪽
ccColor4F const kInk{.12f, .18f, .27f, 1.f};
ccColor4F const kYellow{1.f, .84f, .29f, 1.f};
ccColor4F const kWhite{1.f, 1.f, 1.f, 1.f};
ccColor4F const kTear{.35f, .9f, 1.f, 1.f};
ccColor4F const kMouth{.55f, .12f, .16f, 1.f};

// 둥근 사각형 모양의 얼굴 바탕(외곽선 + tint 채움 + 하이라이트)을 holder에 붙이고
// 그 CCDrawNode를 돌려준다. 표정은 호출부가 이 노드 위에 이어서 그린다.
CCDrawNode* face(CCNode* holder, ccColor4F tint = kYellow) {
    auto* d = CCDrawNode::create();
    auto box = [&](float half, float cut, ccColor4F color) {
        CCPoint corners[] = {{-half + cut, -half}, {half - cut, -half}, {half, -half + cut}, {half, half - cut},
                             {half - cut, half},   {-half + cut, half}, {-half, half - cut}, {-half, -half + cut}};
        d->drawPolygon(corners, 8, color, 0.f, color);
    };
    box(13.5f, 3.2f, kInk);
    box(12.3f, 2.6f, tint);
    d->drawDot({-6.f, 7.f}, 2.2f, {1.f, 1.f, 1.f, .35f});
    holder->addChild(d);
    return d;
}
// points를 순서대로 잇는 선분(꺾은선)을 그린다.
void line(CCDrawNode* d, std::initializer_list<CCPoint> points, float width = 1.2f, ccColor4F color = kInk) {
    CCPoint previous{};
    bool first = true;
    for (auto const& point : points) {
        if (!first) d->drawSegment(previous, point, width, color);
        previous = point;
        first = false;
    }
}
// 표정 부품: 두 눈 / 웃는 입 / 다각형 / 작은 하트 / 눈물방울
void eyes(CCDrawNode* d, float y = 3.f, float r = 1.6f) {
    d->drawDot({-4.5f, y}, r, kInk);
    d->drawDot({4.5f, y}, r, kInk);
}
void smile(CCDrawNode* d, float y = -4.f) {
    line(d, {{-6.f, y + 1.f}, {-3.f, y - 2.f}, {3.f, y - 2.f}, {6.f, y + 1.f}});
}
void poly(CCDrawNode* d, std::initializer_list<CCPoint> points, ccColor4F fill, float border = 0.f,
          ccColor4F ink = kInk) {
    std::vector<CCPoint> list(points);
    d->drawPolygon(list.data(), static_cast<unsigned>(list.size()), fill, border, ink);
}
void miniHeart(CCDrawNode* d, CCPoint at, float k, ccColor4F color) {
    d->drawDot(at + CCPoint{-1.6f * k, 1.f * k}, 1.9f * k, color);
    d->drawDot(at + CCPoint{1.6f * k, 1.f * k}, 1.9f * k, color);
    poly(d, {at + CCPoint{-3.4f * k, .6f * k}, at + CCPoint{3.4f * k, .6f * k}, at + CCPoint{0.f, -3.6f * k}}, color);
}
void drop(CCDrawNode* d, CCPoint at, float k = 1.f) {
    d->drawDot(at, 2.f * k, kTear);
    poly(d, {at + CCPoint{-1.9f * k, .5f * k}, at + CCPoint{1.9f * k, .5f * k}, at + CCPoint{0.f, 4.5f * k}}, kTear);
}
// 글자가 들어간 둥근 사각 배지 (GG, WP 같은 말풍선형 이모트).
CCNode* textBadge(char const* text, ccColor4F tint, float scale) {
    auto* holder = CCNode::create();
    auto* d = CCDrawNode::create();
    poly(d, {{-15.f, -9.f}, {15.f, -9.f}, {15.f, 9.f}, {-15.f, 9.f}}, tint, 1.2f, kInk);
    poly(d, {{-13.f, 4.f}, {13.f, 4.f}, {13.f, 7.5f}, {-13.f, 7.5f}}, {1.f, 1.f, 1.f, .22f});
    holder->addChild(d);
    label(holder, text, {0.f, .5f}, scale, 26.f, ccWHITE, false, "bigFont.fnt");
    return holder;
}
// art::drawGlyph로 그린 공용 아이콘(하트, 왕관 등)을 이모트 크기로 감싼다.
CCNode* glyphEmote(art::Glyph kind, ccColor3B tint, float scale = 1.4f) {
    auto* d = CCDrawNode::create();
    art::drawGlyph(d, kind, tint);
    d->setScale(scale);
    auto* holder = CCNode::create();
    holder->addChild(d);
    return holder;
}
}

// kind(이모트 id)에 맞는 멤버 이모트 노드를 만들어 돌려준다.
// 아이콘형 -> 글자 배지형 -> 개별 도형형 순으로 처리하고, 나머지는 얼굴 바탕 위에 표정을 그린다.
CCNode* memberEmoteArt(std::string const& kind) {
    using namespace emote_art;
    auto* holder = CCNode::create();
    // 공용 아이콘과 글자 배지로 끝나는 이모트
    if (kind == "heart") return glyphEmote(art::Glyph::Heart, ccc3(255, 70, 100));
    if (kind == "crown") return glyphEmote(art::Glyph::Crown, ccc3(255, 205, 60));
    if (kind == "trophy") return glyphEmote(art::Glyph::Trophy, ccc3(255, 196, 50));
    if (kind == "star") return glyphEmote(art::Glyph::Star, ccc3(255, 222, 70), 1.45f);
    if (kind == "bolt") return glyphEmote(art::Glyph::Bolt, ccc3(255, 225, 60));
    if (kind == "skull") return glyphEmote(art::Glyph::Skull, ccc3(235, 235, 240));
    if (kind == "gg") return textBadge("GG", {.2f, .62f, 1.f, 1.f}, .55f);
    if (kind == "wp") return textBadge("WP", {.25f, .75f, .4f, 1.f}, .55f);
    if (kind == "gl") return textBadge("GL", {.6f, .35f, .95f, 1.f}, .55f);
    if (kind == "lol") return textBadge("LOL", {1.f, .55f, .15f, 1.f}, .45f);
    if (kind == "hi") return textBadge("HI!", {.15f, .72f, .78f, 1.f}, .5f);
    // 얼굴 바탕 없이 따로 그리는 이모트 (100, ?!, RIP, eyes, swords, ghost)
    if (kind == "hundred") {
        label(holder, "100", {0.f, 2.f}, .62f, 30.f, ccc3(255, 60, 70), false, "bigFont.fnt");
        auto* d = CCDrawNode::create();
        line(d, {{-13.f, -9.f}, {13.f, -9.f}}, 1.2f, {1.f, .24f, .28f, 1.f});
        line(d, {{-11.f, -12.f}, {11.f, -12.f}}, 1.2f, {1.f, .24f, .28f, 1.f});
        holder->addChild(d, -1);
        return holder;
    }
    if (kind == "what") {
        label(holder, "?!", {0.f, 0.f}, .85f, 30.f, ccc3(255, 150, 40), false, "bigFont.fnt");
        return holder;
    }
    if (kind == "rip") {
        auto* d = CCDrawNode::create();
        poly(d, {{-10.f, -13.f}, {10.f, -13.f}, {10.f, 4.f}, {7.f, 10.f}, {0.f, 13.f}, {-7.f, 10.f}, {-10.f, 4.f}},
             {.62f, .66f, .72f, 1.f}, 1.2f, kInk);
        poly(d, {{-14.f, -15.f}, {14.f, -15.f}, {14.f, -12.f}, {-14.f, -12.f}}, {.35f, .65f, .3f, 1.f});
        holder->addChild(d);
        label(holder, "RIP", {0.f, 1.f}, .32f, 18.f, ccc3(40, 46, 60), false, "bigFont.fnt");
        return holder;
    }
    if (kind == "eyes") {
        auto* d = CCDrawNode::create();
        for (float x : {-6.5f, 6.5f}) {
            d->drawDot({x, 0.f}, 7.f, kInk);
            d->drawDot({x, 0.f}, 6.f, kWhite);
            d->drawDot({x + 2.6f, -1.f}, 2.8f, kInk);
            d->drawDot({x + 3.4f, 0.f}, .9f, kWhite);
        }
        holder->addChild(d);
        return holder;
    }
    if (kind == "swords") {
        auto* d = CCDrawNode::create();
        art::sword(d, ccc3(230, 236, 250), -45.f, 1.25f);
        art::sword(d, ccc3(255, 214, 120), 45.f, 1.25f);
        holder->addChild(d);
        return holder;
    }
    if (kind == "ghost") {
        auto* d = CCDrawNode::create();
        d->drawDot({0.f, 3.f}, 11.f, kInk);
        d->drawDot({0.f, 3.f}, 10.f, kWhite);
        poly(d,
             {{-11.f, 3.f},
              {11.f, 3.f},
              {11.f, -12.f},
              {7.f, -9.f},
              {3.5f, -13.f},
              {0.f, -9.f},
              {-3.5f, -13.f},
              {-7.f, -9.f},
              {-11.f, -12.f}},
             kWhite, 1.1f, kInk);
        poly(d, {{-9.9f, 3.f}, {9.9f, 3.f}, {9.9f, -2.f}, {-9.9f, -2.f}}, kWhite);
        d->drawDot({-3.8f, 4.f}, 2.f, kInk);
        d->drawDot({3.8f, 4.f}, 2.f, kInk);
        d->drawDot({0.f, -2.5f}, 1.8f, kInk);
        holder->addChild(d);
        return holder;
    }
    // 나머지는 얼굴 바탕 위에 표정을 올리는 이모트. devil/cold/hot은 얼굴색이 다르다.
    ccColor4F tint = kYellow;
    if (kind == "devil")
        tint = {.62f, .32f, .9f, 1.f};
    else if (kind == "cold")
        tint = {.55f, .82f, 1.f, 1.f};
    else if (kind == "hot")
        tint = {1.f, .42f, .3f, 1.f};
    if (kind == "devil") {
        auto* horns = CCDrawNode::create();
        poly(horns, {{-11.f, 6.f}, {-5.f, 10.f}, {-12.f, 16.f}}, tint, 1.f, kInk);
        poly(horns, {{11.f, 6.f}, {5.f, 10.f}, {12.f, 16.f}}, tint, 1.f, kInk);
        holder->addChild(horns);
    }
    auto* d = face(holder, tint);
    // kind별 표정. 해당하지 않으면 마지막 else의 기본 웃는 얼굴이 된다.
    if (kind == "laugh") {
        line(d, {{-7.f, 3.f}, {-4.5f, 6.f}, {-2.f, 3.f}});
        line(d, {{2.f, 3.f}, {4.5f, 6.f}, {7.f, 3.f}});
        poly(d, {{-7.5f, -2.f}, {7.5f, -2.f}, {5.f, -7.5f}, {0.f, -9.5f}, {-5.f, -7.5f}}, kMouth, 1.f, kInk);
        drop(d, {-12.f, 0.f});
        drop(d, {12.f, 0.f});
    } else if (kind == "cool") {
        poly(d, {{-11.f, 6.f}, {-1.5f, 6.f}, {-2.5f, 0.f}, {-9.5f, 0.f}}, kInk);
        poly(d, {{11.f, 6.f}, {1.5f, 6.f}, {2.5f, 0.f}, {9.5f, 0.f}}, kInk);
        line(d, {{-1.5f, 5.f}, {1.5f, 5.f}});
        d->drawDot({-8.f, 4.f}, 1.f, {1.f, 1.f, 1.f, .6f});
        line(d, {{-4.f, -6.f}, {1.f, -6.5f}, {6.f, -3.5f}});
    } else if (kind == "wow") {
        for (float x : {-5.f, 5.f}) {
            d->drawDot({x, 3.f}, 3.f, kWhite);
            d->drawDot({x, 3.f}, 1.6f, kInk);
        }
        d->drawDot({0.f, -6.f}, 3.4f, kInk);
        d->drawDot({0.f, -6.f}, 2.2f, kMouth);
    } else if (kind == "cry") {
        line(d, {{-7.f, 4.f}, {-2.5f, 3.f}});
        line(d, {{2.5f, 3.f}, {7.f, 4.f}});
        line(d, {{-5.f, -8.f}, {-2.f, -5.5f}, {2.f, -5.5f}, {5.f, -8.f}});
        poly(d, {{-6.f, 2.f}, {-3.f, 2.f}, {-3.f, -12.f}, {-6.f, -12.f}}, kTear);
        poly(d, {{3.f, 2.f}, {6.f, 2.f}, {6.f, -12.f}, {3.f, -12.f}}, kTear);
    } else if (kind == "love") {
        miniHeart(d, {-5.f, 3.f}, 1.f, {1.f, .25f, .4f, 1.f});
        miniHeart(d, {5.f, 3.f}, 1.f, {1.f, .25f, .4f, 1.f});
        smile(d);
    } else if (kind == "wink") {
        d->drawDot({-4.5f, 3.f}, 1.6f, kInk);
        line(d, {{2.f, 3.f}, {7.f, 3.f}});
        smile(d);
    } else if (kind == "tongue") {
        eyes(d);
        line(d, {{-6.f, -4.f}, {6.f, -4.f}});
        d->drawDot({2.5f, -7.f}, 3.f, kInk);
        d->drawDot({2.5f, -7.f}, 2.2f, {1.f, .45f, .55f, 1.f});
        poly(d, {{-.5f, -4.4f}, {5.5f, -4.4f}, {5.5f, -7.f}, {-.5f, -7.f}}, {1.f, .45f, .55f, 1.f});
    } else if (kind == "sweat") {
        eyes(d);
        smile(d);
        drop(d, {9.5f, 6.f}, 1.2f);
    } else if (kind == "sleep") {
        line(d, {{-7.f, 3.f}, {-2.f, 3.f}});
        line(d, {{2.f, 3.f}, {7.f, 3.f}});
        d->drawDot({0.f, -6.f}, 1.8f, kInk);
        label(holder, "z", {11.f, 11.f}, .38f, 12.f, ccc3(170, 200, 255), false, "bigFont.fnt");
    } else if (kind == "think") {
        line(d, {{-7.f, 6.5f}, {-2.5f, 8.f}});
        line(d, {{2.5f, 6.f}, {7.f, 6.f}});
        eyes(d);
        line(d, {{-4.f, -6.f}, {5.f, -4.5f}});
        label(holder, "?", {12.f, 11.f}, .42f, 12.f, ccc3(255, 214, 120), false, "bigFont.fnt");
    } else if (kind == "devil") {
        line(d, {{-7.f, 7.f}, {-2.5f, 4.5f}});
        line(d, {{2.5f, 4.5f}, {7.f, 7.f}});
        eyes(d, 2.5f);
        poly(d, {{-7.f, -3.f}, {7.f, -3.f}, {0.f, -9.f}}, kMouth, 1.f, kInk);
    } else if (kind == "angel") {
        auto* halo = CCDrawNode::create();
        art::stroke(halo, art::circle({0.f, 0.f}, 7.f, 24), 1.4f, {1.f, .92f, .4f, 1.f});
        halo->setScaleY(.35f);
        halo->setPosition({0.f, 15.f});
        holder->addChild(halo, 2);
        line(d, {{-7.f, 3.f}, {-4.5f, 5.f}, {-2.f, 3.f}});
        line(d, {{2.f, 3.f}, {4.5f, 5.f}, {7.f, 3.f}});
        smile(d);
    } else if (kind == "dizzy") {
        for (float x : {-4.5f, 4.5f}) {
            line(d, {{x - 2.f, 5.f}, {x + 2.f, 1.f}});
            line(d, {{x - 2.f, 1.f}, {x + 2.f, 5.f}});
        }
        line(d, {{-6.f, -6.f}, {-3.f, -4.f}, {0.f, -6.f}, {3.f, -4.f}, {6.f, -6.f}});
    } else if (kind == "party") {
        auto* hat = CCDrawNode::create();
        poly(hat, {{-9.f, 8.f}, {1.f, 12.f}, {-8.f, 22.f}}, {.95f, .3f, .55f, 1.f}, 1.f, kInk);
        hat->drawDot({-8.f, 22.f}, 2.f, {1.f, .9f, .3f, 1.f});
        holder->addChild(hat, 2);
        eyes(d);
        poly(d, {{-6.f, -3.f}, {6.f, -3.f}, {0.f, -8.f}}, kMouth, 1.f, kInk);
        for (auto [x, y, c] : {std::tuple{11.f, 10.f, ccColor4F{.3f, .8f, 1.f, 1.f}},
                               std::tuple{13.f, -2.f, ccColor4F{1.f, .85f, .2f, 1.f}},
                               std::tuple{-13.f, -6.f, ccColor4F{.5f, 1.f, .5f, 1.f}}})
            d->drawDot({x, y}, 1.3f, c);
    } else if (kind == "cold") {
        eyes(d);
        poly(d, {{-6.f, -3.f}, {6.f, -3.f}, {6.f, -8.f}, {-6.f, -8.f}}, kWhite, .8f, kInk);
        for (float x : {-3.f, 0.f, 3.f}) line(d, {{x, -3.f}, {x, -8.f}}, .5f);
        line(d, {{-6.f, -5.5f}, {6.f, -5.5f}}, .5f);
        poly(d, {{-10.f, 10.f}, {-7.f, 10.f}, {-8.5f, 5.f}}, kWhite);
        poly(d, {{7.f, 10.f}, {10.f, 10.f}, {8.5f, 6.f}}, kWhite);
    } else if (kind == "hot") {
        line(d, {{-7.f, 5.f}, {-2.5f, 3.f}});
        line(d, {{2.5f, 3.f}, {7.f, 5.f}});
        eyes(d, 1.5f, 1.3f);
        poly(d, {{-6.f, -3.f}, {6.f, -3.f}, {4.f, -9.f}, {-4.f, -9.f}}, kMouth, 1.f, kInk);
        d->drawDot({0.f, -8.f}, 2.2f, {1.f, .5f, .6f, 1.f});
        drop(d, {11.f, 4.f});
        drop(d, {-11.f, 7.f}, .8f);
    } else {
        eyes(d);
        smile(d);
    }
    return holder;
}
