#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCParticleSystemQuad.hpp>
#include <Geode/modify/CCMotionStreak.hpp>
#include <cmath>

using namespace geode::prelude;

namespace {
    // Settings are cached here because the draw hooks run many times per frame.
    // A settings lookup every draw call would cost more than it saves.
    bool s_noParticles = false;
    bool s_noTrails = false;

    // The game's own frame interval, saved before we change it so it can be restored.
    double s_originalInterval = 1.0 / 60.0;
    bool s_fpsChanged = false;

    void applyFrameRate(bool enabled) {
        auto director = CCDirector::sharedDirector();
        if (!director) return;

        auto mod = Mod::get();
        const double current = director->getAnimationInterval();

        if (enabled && mod->getSettingValue<bool>("unlock-fps")) {
            if (!s_fpsChanged && current > 0.0) s_originalInterval = current;

            const int64_t fps = std::max<int64_t>(1, mod->getSettingValue<int64_t>("target-fps"));
            const double target = 1.0 / static_cast<double>(fps);

            // Only touch the director when the value actually differs, because
            // changing the interval restarts the render loop.
            if (std::fabs(current - target) > 1e-6) {
                director->setAnimationInterval(target);
            }
            s_fpsChanged = true;
        } else if (s_fpsChanged) {
            director->setAnimationInterval(s_originalInterval);
            s_fpsChanged = false;
        }
    }

    // Re-reads every setting and applies it. Cheap, so it is safe to call often.
    void refreshSettings() {
        auto mod = Mod::get();
        const bool enabled = mod->getSettingValue<bool>("enabled");
        s_noParticles = enabled && mod->getSettingValue<bool>("no-particles");
        s_noTrails = enabled && mod->getSettingValue<bool>("no-trails");
        applyFrameRate(enabled);
    }
}

// Apply as soon as the mod loads.
$on_mod(Loaded) {
    refreshSettings();
}

// Main menu: the game may have reset its frame interval while loading.
class $modify(FPSBoostMenu, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        refreshSettings();
        return true;
    }
};

// Particles: skip the draw call (vertex upload + GPU draw) while inside a level.
class $modify(FPSBoostParticles, CCParticleSystemQuad) {
    void draw() {
        if (s_noParticles && PlayLayer::get()) return;
        CCParticleSystemQuad::draw();
    }
};

// Trails: same idea for streak trails.
class $modify(FPSBoostTrails, CCMotionStreak) {
    void draw() {
        if (s_noTrails && PlayLayer::get()) return;
        CCMotionStreak::draw();
    }
};

class $modify(FPSBoostPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        refreshSettings();

        auto mod = Mod::get();
        if (mod->getSettingValue<bool>("enabled") && mod->getSettingValue<bool>("no-background")) {
            if (m_background) m_background->setVisible(false);
        }
    }
};

// Pause menu: a button that opens this mod's settings, and a re-apply on resume
// so changes made from that popup take effect immediately.
class $modify(FPSBoostPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        if (!Mod::get()->getSettingValue<bool>("settings-button")) return;

        auto spr = ButtonSprite::create("FPS Boost");
        spr->setScale(0.6f);
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(FPSBoostPause::onFPSBoostSettings)
        );
        auto menu = CCMenu::create();
        menu->addChild(btn);
        menu->setPosition({62.f, 52.f});
        this->addChild(menu);
    }

    void onFPSBoostSettings(CCObject*) {
        openSettingsPopup(Mod::get());
    }

    void onResume(CCObject* sender) {
        PauseLayer::onResume(sender);
        refreshSettings();
    }
};
