#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
using namespace geode::prelude;

static bool g_attacking = false;

// swap every pause button's icon to a custom one and disable it
static void ransomifyButtons(CCNode* node) {
    for (auto child : CCArrayExt<CCNode*>(node->getChildren())) {
        if (auto btn = typeinfo_cast<CCMenuItemSpriteExtra*>(child)) {
            auto orig = btn->getContentSize();
            auto name = fmt::format("{}/ransom-{}.png",
                std::string_view(Mod::get()->getID()), std::string_view(btn->getID()));
            auto spr = CCSprite::create(name.c_str());
            if (!spr) spr = CCSprite::create("ransom-button.png"_spr);
            if (spr && orig.width > 0 && orig.height > 0) {
                auto holder = CCNode::create();
                holder->setContentSize(orig);
                spr->setScale(std::min(orig.width / spr->getContentWidth(), orig.height / spr->getContentHeight()));
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
        bool done = false;
        CCLabelBMFont* label = nullptr;
        CCMenu* coins = nullptr;
    };

    void customSetup() {
        PauseLayer::customSetup();
        if (!g_attacking) return;
        auto f = m_fields.self();
        auto win = CCDirector::get()->getWinSize();

        f->total = static_cast<int>(std::max<int64_t>(1, Mod::get()->getSettingValue<int64_t>("coins")));
        f->left = f->total;
        f->time = static_cast<float>(Mod::get()->getSettingValue<double>("time-limit"));

        ransomifyButtons(this);

        this->addChild(CCLayerColor::create({120, 0, 0, 90}), 99);

        f->coins = CCMenu::create();
        f->coins->setPosition({0, 0});
        this->addChild(f->coins, 100);

        f->label = CCLabelBMFont::create("", "bigFont.fnt");
        f->label->setScale(.5f);
        f->label->setPosition({win.width / 2, win.height - 20.f});
        this->addChild(f->label, 100);

        spawnCoin();
        updateLabel();
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
        if (--f->left <= 0) return ransomFinish(true);
        spawnCoin();
        updateLabel();
    }

    void ransomTick(float dt) {
        auto f = m_fields.self();
        if (f->done) return;
        if ((f->time -= dt) <= 0.f) return ransomFinish(false);
        updateLabel();
    }

    void updateLabel() {
        auto f = m_fields.self();
        f->label->setString(fmt::format("{}/{}  |  {:.1f}s",
            f->total - f->left, f->total, std::max(f->time, 0.f)).c_str());
    }

    void ransomFinish(bool won) {
        m_fields->done = true;
        this->unschedule(schedule_selector(RansomPause::ransomTick));
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
        f->spr = CCSprite::create("ransom.png"_spr);
        if (!f->spr) return;
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
