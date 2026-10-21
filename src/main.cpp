#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

#ifdef GEODE_IS_DESKTOP
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#endif

using namespace geode::prelude;

namespace {
    struct Stage {
        int allTicks;
        int waveTicks;
    };

    constexpr Stage stages[] = {
        {2, 2},
        {4, 4},
        {8, 8},
        {16, 8}
    };
    constexpr int stageCount = 4;

    bool isEnabled() {
        return Mod::get()->getSavedValue<bool>("enabled", true);
    }

    int getStage() {
        int s = Mod::get()->getSavedValue<int>("stage", 0);
        return (s < 0 || s >= stageCount) ? 0 : s;
    }

    void setEnabled(bool value) {
        Mod::get()->setSavedValue<bool>("enabled", value);
    }

    void setStage(int value) {
        Mod::get()->setSavedValue<int>("stage", value % stageCount);
    }

    std::string stageText() {
        auto const& s = stages[getStage()];
        return fmt::format("{}/{} ticks", s.allTicks, s.waveTicks);
    }

    void nextStage() {
        setStage(getStage() + 1);
        log::info("[InputBuffer] Stage {}: {} Ticks All | {} Ticks Wave",
            getStage() + 1, stages[getStage()].allTicks, stages[getStage()].waveTicks);
    }
}

class $modify(PredictiveBufferPause, PauseLayer) {
    struct Fields {
        ButtonSprite* stageSprite = nullptr;
    };

    void customSetup() {
        PauseLayer::customSetup();

        auto menu = CCMenu::create();
        menu->setID("input-buffer-menu"_spr);
        menu->setPosition({0.f, 0.f});
        menu->setContentSize(CCDirector::get()->getWinSize());

        // Toggle on/off
        auto toggler = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(PredictiveBufferPause::onToggle), 0.7f
        );
        toggler->toggle(isEnabled());
        toggler->setPosition({30.f, 60.f});
        menu->addChild(toggler);

        auto label = CCLabelBMFont::create("Input Buffer", "bigFont.fnt");
        label->setScale(0.45f);
        label->setAnchorPoint({0.f, 0.5f});
        label->setPosition({50.f, 60.f});
        menu->addChild(label);

        // Stage button (tap to cycle)
        auto spr = ButtonSprite::create(stageText().c_str());
        spr->setScale(0.6f);
        m_fields->stageSprite = spr;
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(PredictiveBufferPause::onStage)
        );
        btn->setPosition({85.f, 30.f});
        menu->addChild(btn);

        this->addChild(menu, 100);
    }

    void onToggle(CCObject* sender) {
        // GD calls the callback before flipping the state
        auto t = static_cast<CCMenuItemToggler*>(sender);
        setEnabled(!t->isToggled());
    }

    void onStage(CCObject*) {
        nextStage();
        if (m_fields->stageSprite) {
            m_fields->stageSprite->setString(stageText().c_str());
        }
    }
};

#ifdef GEODE_IS_DESKTOP
class $modify(PredictiveInputBufferKeyboard, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double timestamp) {
        if (key == enumKeyCodes::KEY_F6 && isKeyDown && !isKeyRepeat) {
            nextStage();
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, timestamp);
    }
};
#endif

$on_mod(Loaded) {
    log::info("[InputBuffer] Loaded. enabled={} stage={}", isEnabled(), getStage() + 1);
}

// ---- Real buffer logic ----
// If the player presses jump in the air and releases before landing,
// GD drops the click. Here we remember it for N ticks and fire it on landing.
class $modify(PredictiveBufferPlayer, PlayerObject) {
    struct Fields {
        int pending = 0;          // ticks left in the buffer window
        bool released = false;    // button was released while pending
        bool releaseNext = false; // release the synthetic click next tick
    };

    bool groundMode() {
        return !(m_isShip || m_isBird || m_isDart || m_isSwing);
    }

    bool pushButton(PlayerButton btn) {
        if (btn == PlayerButton::Jump && isEnabled() && PlayLayer::get()
            && !m_isOnGround && groundMode()) {
            m_fields->pending = stages[getStage()].allTicks;
            m_fields->released = false;
        }
        return PlayerObject::pushButton(btn);
    }

    bool releaseButton(PlayerButton btn) {
        if (btn == PlayerButton::Jump && m_fields->pending > 0) {
            m_fields->released = true;
        }
        return PlayerObject::releaseButton(btn);
    }

        void update(float dt) {
        PlayerObject::update(dt);

        if (m_fields->releaseNext) {
            m_fields->releaseNext = false;
            PlayerObject::releaseButton(PlayerButton::Jump);
        }

        if (m_fields->pending > 0) {
            if (!isEnabled() || !groundMode()) {
                m_fields->pending = 0;
            } else if (m_isOnGround) {
                // Landed. If still held, GD already jumps by itself.
                if (m_fields->released) {
                    PlayerObject::pushButton(PlayerButton::Jump);
                    m_fields->releaseNext = true;
                }
                m_fields->pending = 0;
            } else {
                m_fields->pending--;
            }
        }
    }
        PlayerObject::update(dt);

        auto& f = *m_fields;

        if (f.releaseNext) {
            f.releaseNext = false;
            PlayerObject::releaseButton(PlayerButton::Jump);
        }

        if (f.pending > 0) {
            if (!isEnabled() || !groundMode()) {
                f.pending = 0;
            } else if (m_isOnGround) {
                // Landed. If still held, GD already jumps by itself.
                if (f.released) {
                    PlayerObject::pushButton(PlayerButton::Jump);
                    f.releaseNext = true;
                }
                f.pending = 0;
            } else {
                f.pending--;
            }
        }
    }

    void resetObject() {
        PlayerObject::resetObject();
        m_fields->pending = 0;
        m_fields->released = false;
        m_fields->releaseNext = false;
    }
};
