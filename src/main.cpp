#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
using namespace geode::prelude;

static bool g_attacking = false;

constexpr int ATTACK_FRAMES = 34;
constexpr float ATTACK_FRAME_TIME = 0.1f;

// resources are packed into the mod's spritesheet; null if the frame is missing
static CCSprite* modSprite(char const* frame) {
    if (!CCSpriteFrameCache::get()->spriteFrameByName(frame)) return nullptr;
    return CCSprite::createWithSpriteFrameName(frame);
}

static void playSfx(char const* file) {
    auto path = Mod::get()->getResourcesDir() / file;
    FMODAudioEngine::sharedEngine()->playEffect(path.string());
}

// scale a node so it fits inside w x h, keeping its aspect ratio
static void fitTo(CCNode* node, float w, float h) {
    auto size = node->getContentSize();
    if (size.width > 0 && size.height > 0)
        node->setScale(std::min(w / size.width, h / size.height));
}

// swap every pause button's icon to the stop sign and disable it
static void ransomifyButtons(CCNode* node) {
    for (auto child : CCArrayExt<CCNode*>(node->getChildren())) {
        if (auto btn = typeinfo_cast<CCMenuItemSpriteExtra*>(child)) {
            auto orig = btn->getContentSize();
            auto name = fmt::format("{}/ransom-{}.png",
                std::string_view(Mod::get()->getID()), std::string_view(btn->getID()));
            auto spr = modSprite(name.c_str());
            if (!spr) spr = modSprite("ransom-button.png"_spr);
            if (spr && orig.width > 0 && orig.height > 0) {
                auto holder = CCNode::create();
                holder->setContentSize(orig);
                fitTo(spr, orig.width, orig.height);
                spr->setPosition(orig / 2);
                holder->addChild(spr);
                btn->setNormalImage(holder);
            }
            btn->setEnabled(false);
            continue;
        }
        if (auto item = typeinfo_cast<CCMenuItem*>(child)) {
            item->setEnabled(false);
            continue;
        }
        ransomifyButtons(child);
    }
}

// ---------- pause menu minigame ----------
class $modify(RansomPause, PauseLayer) {
    struct Fields {
        int total = 20;
        int left = 20;
        float time = 10.f;
        bool started = false;
        bool done = false;
        bool won = false;
        CCLabelBMFont* coinLabel = nullptr;
        CCLabelBMFont* timeLabel = nullptr;
        CCMenu* coins = nullptr;
    };

    void customSetup() {
        PauseLayer::customSetup();
        if (!g_attacking) return;
        log::info("Ransom: pause menu encrypted");
        auto f = m_fields.self();

        f->total = static_cast<int>(std::max<int64_t>(1, Mod::get()->getSettingValue<int64_t>("coins")));
        f->left = f->total;
        f->time = static_cast<float>(Mod::get()->getSettingValue<double>("time-limit"));

        ransomifyButtons(this);
        this->addChild(CCLayerColor::create({120, 0, 0, 90}), 99);

        playAttack();
    }

    // full-screen attack animation, then the ransom note
    void playAttack() {
        auto win = CCDirector::get()->getWinSize();
        auto frames = CCArray::create();
        for (int i = 0; i < ATTACK_FRAMES; i++) {
            auto name = fmt::format("{}/attack_{:02}.png",
                std::string_view(Mod::get()->getID()), i);
            if (auto frame = CCSpriteFrameCache::get()->spriteFrameByName(name.c_str()))
                frames->addObject(frame);
        }

        playSfx("attack.ogg");

        if (frames->count() == 0) {
            log::error("Ransom: attack animation frames missing, skipping it");
            startRansom();
            return;
        }

        auto anim = CCSprite::createWithSpriteFrame(
            static_cast<CCSpriteFrame*>(frames->objectAtIndex(0)));
        anim->setPosition(win / 2);
        auto size = anim->getContentSize();
        anim->setScaleX(win.width / size.width);
        anim->setScaleY(win.height / size.height);
        this->addChild(anim, 200);

        anim->runAction(CCSequence::create(
            CCAnimate::create(CCAnimation::createWithSpriteFrames(frames, ATTACK_FRAME_TIME)),
            CCRemoveSelf::create(),
            nullptr));
        this->runAction(CCSequence::create(
            CCDelayTime::create(frames->count() * ATTACK_FRAME_TIME),
            CCCallFunc::create(this, callfunc_selector(RansomPause::startRansom)),
            nullptr));
    }

    void startRansom() {
        auto f = m_fields.self();
        if (f->started) return;
        f->started = true;
        auto win = CCDirector::get()->getWinSize();

        auto note = modSprite("encryption.png"_spr);
        if (note) {
            fitTo(note, win.width * .7f, win.height * .6f);
            note->setPosition(win / 2);
            this->addChild(note, 100);

            // cover the note's printed "500" and "01:30" with our live values
            auto size = note->getContentSize();
            auto cover = [&](float x0, float x1) {
                auto rect = CCLayerColor::create({18, 12, 12, 255},
                    size.width * (x1 - x0), size.height * .14f);
                rect->setPosition({size.width * x0, size.height * .05f});
                note->addChild(rect);
            };
            cover(.03f, .21f);
            cover(.68f, .975f);

            f->coinLabel = CCLabelBMFont::create("", "bigFont.fnt");
            f->coinLabel->setColor({255, 210, 60});
            f->coinLabel->setPosition({size.width * .12f, size.height * .12f});
            note->addChild(f->coinLabel);

            f->timeLabel = CCLabelBMFont::create("", "bigFont.fnt");
            f->timeLabel->setColor({255, 40, 40});
            f->timeLabel->setPosition({size.width * .83f, size.height * .12f});
            note->addChild(f->timeLabel);
        } else {
            log::error("Ransom: couldn't load encryption.png, using plain labels");
            f->coinLabel = CCLabelBMFont::create("", "bigFont.fnt");
            f->coinLabel->setPosition({win.width / 2 - 60.f, win.height - 20.f});
            this->addChild(f->coinLabel, 100);
            f->timeLabel = CCLabelBMFont::create("", "bigFont.fnt");
            f->timeLabel->setPosition({win.width / 2 + 60.f, win.height - 20.f});
            this->addChild(f->timeLabel, 100);
        }

        f->coins = CCMenu::create();
        f->coins->setPosition({0, 0});
        this->addChild(f->coins, 101);

        spawnCoin();
        updateLabels();
        this->schedule(schedule_selector(RansomPause::ransomTick));
    }

    void spawnCoin() {
        auto win = CCDirector::get()->getWinSize();
        auto btn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_coinsIcon_001.png"),
            this, menu_selector(RansomPause::onRansomCoin));
        btn->setPosition({
            40.f + rand() % (int)(win.width - 80),
            40.f + rand() % (int)(win.height - 100)
        });
        m_fields->coins->addChild(btn);
    }

    void onRansomCoin(CCObject* sender) {
        auto f = m_fields.self();
        if (f->done) return;
        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
        btn->setEnabled(false);
        btn->setVisible(false);
        playSfx("coin.mp3");
        if (--f->left <= 0) return ransomFinish(true);
        spawnCoin();
        updateLabels();
    }

    void ransomTick(float dt) {
        auto f = m_fields.self();
        if (f->done) return;
        if ((f->time -= dt) <= 0.f) return ransomFinish(false);
        updateLabels();
    }

    void updateLabels() {
        auto f = m_fields.self();
        auto t = std::max(f->time, 0.f);
        // amount still owed, like the note's ransom box
        f->coinLabel->setString(std::to_string(f->left).c_str());
        f->timeLabel->setString(fmt::format("{:02}:{:04.1f}",
            static_cast<int>(t) / 60, std::fmod(t, 60.f)).c_str());
        if (auto note = f->coinLabel->getParent(); note != this) {
            auto size = note->getContentSize();
            fitTo(f->coinLabel, size.width * .17f, size.height * .12f);
            fitTo(f->timeLabel, size.width * .28f, size.height * .12f);
        } else {
            f->coinLabel->setScale(.5f);
            f->timeLabel->setScale(.5f);
        }
    }

    void ransomFinish(bool won) {
        auto f = m_fields.self();
        log::info("Ransom: {}", won ? "paid, resuming" : "out of time, killing player");
        f->done = true;
        f->won = won;
        this->unschedule(schedule_selector(RansomPause::ransomTick));
        if (f->coins) f->coins->setEnabled(false);

        float delay = 0.f;
        if (won) {
            playSfx("paid.ogg");
        } else {
            playSfx("fail.mp3");
            if (auto face = modSprite("ransom.png"_spr)) {
                auto win = CCDirector::get()->getWinSize();
                fitTo(face, win.width, win.height);
                face->setPosition(win / 2);
                face->setScale(face->getScale() * .3f);
                face->runAction(CCEaseIn::create(
                    CCScaleBy::create(.25f, 1.f / .3f), 2.f));
                this->addChild(face, 300);
            }
            delay = .8f;
        }
        this->runAction(CCSequence::create(
            CCDelayTime::create(delay),
            CCCallFunc::create(this, callfunc_selector(RansomPause::ransomDone)),
            nullptr));
    }

    void ransomDone() {
        bool won = m_fields->won;
        g_attacking = false;
        Ref<PauseLayer> self = this;
        queueInMainThread([self, won] {
            auto pl = PlayLayer::get();
            self->onResume(nullptr);
            if (!won && pl && !pl->m_player1->m_isDead)
                pl->destroyPlayer(pl->m_player1, nullptr);
        });
    }

    // block every way out while attacking
    void onResume(CCObject* s)      { if (!g_attacking) PauseLayer::onResume(s); }
    void onRestart(CCObject* s)     { if (!g_attacking) PauseLayer::onRestart(s); }
    void onRestartFull(CCObject* s) { if (!g_attacking) PauseLayer::onRestartFull(s); }
    void onQuit(CCObject* s)        { if (!g_attacking) PauseLayer::onQuit(s); }
    void keyBackClicked()           { if (!g_attacking) PauseLayer::keyBackClicked(); }
    void keyDown(enumKeyCodes k, double t) { if (!g_attacking) PauseLayer::keyDown(k, t); }
};

// ---------- entity ----------
class $modify(RansomPL, PlayLayer) {
    struct Fields {
        bool armed = false, shown = false;
        float timer = 0.f, at = 0.f, visibleFor = 0.f;
        CCSprite* spr = nullptr;
    };

    void rollAttempt() {
        auto f = m_fields.self();
        hideRansom();
        auto chance = Mod::get()->getSettingValue<int64_t>("chance");
        f->armed = chance > 0 && rand() % chance == 0;
        f->timer = 0.f;
        f->at = 1.f + (rand() % 1400) / 100.f; // 1-15s into the attempt
        if (f->armed)
            log::info("Ransom: armed, appears {:.2f}s into this attempt", f->at);
        else
            log::debug("Ransom: not this attempt (1 in {})", chance);
    }

    bool init(GJGameLevel* l, bool a, bool b) {
        if (!PlayLayer::init(l, a, b)) return false;
        rollAttempt();
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        rollAttempt();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto f = m_fields.self();
        if (g_attacking || m_player1->m_isDead) return;
        if (f->shown) {
            if ((f->visibleFor -= dt) <= 0.f) hideRansom();
            return;
        }
        if (f->armed && (f->timer += dt) >= f->at) {
            f->armed = false;
            showRansom();
        }
    }

    void showRansom() {
        auto f = m_fields.self();
        auto win = CCDirector::get()->getWinSize();
        f->spr = modSprite("ransom.png"_spr);
        if (!f->spr) {
            log::error("Ransom: couldn't load ransom.png from the mod spritesheet");
            return;
        }
        log::info("Ransom: shown");
        playSfx("spawn.ogg");
        fitTo(f->spr, win.height * .4f, win.height * .4f);
        f->spr->setPosition({
            60.f + rand() % (int)(win.width - 120),
            60.f + rand() % (int)(win.height - 120)
        });
        this->addChild(f->spr, 1000);
        f->shown = true;
        f->visibleFor = 0.5f;
    }

    void hideRansom() {
        auto f = m_fields.self();
        if (f->spr) { f->spr->removeFromParent(); f->spr = nullptr; }
        f->shown = false;
    }

    void onInput() {
        if (!m_fields->shown || g_attacking) return;
        log::info("Ransom: input while visible, attacking");
        hideRansom();
        g_attacking = true;
        queueInMainThread([this] {
            this->pauseGame(false);
            if (!CCScene::get()->getChildByType<PauseLayer>(0))
                g_attacking = false; // pause failed, don't softlock
        });
    }
};

// ---------- input ----------
class $modify(GJBaseGameLayer) {
    void handleButton(bool down, int button, bool p1) {
        GJBaseGameLayer::handleButton(down, button, p1);
        if (!down) return;
        auto pl = PlayLayer::get();
        if (pl && static_cast<GJBaseGameLayer*>(pl) == this)
            static_cast<RansomPL*>(pl)->onInput();
    }
};
