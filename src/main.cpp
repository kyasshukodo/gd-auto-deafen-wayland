#define WIN32_LEAN_AND_MEAN
#include <winsock.h>

#include <Geode/Geode.hpp>
#include <Geode/Bindings.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/loader/Mod.hpp>

using namespace geode::prelude;

// ---------------- Networking helper ----------------

bool sendHelperCommand(const char* cmd) {
    WSADATA wsaData;
    // Winsock 1.x init
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
        log::error("AutoDeafen: WSAStartup failed");
        return false;
    }

    // Ensure WSACleanup is called on all exit paths
    bool success = false;
    SOCKET sock = INVALID_SOCKET;

    do {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            log::error("AutoDeafen: socket() failed");
            break;
        }

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(44555);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        log::info("AutoDeafen: connecting to helper at 127.0.0.1:44555");

        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            log::error("AutoDeafen: connect() to helper failed");
            break;
        }

        // Send just the command and a newline, e.g. "DEAFEN\n"
        std::string line = cmd;
        line.push_back('\n');

        int len  = static_cast<int>(line.size());
        int sent = send(sock, line.c_str(), len, 0);

        if (sent != len) {
            log::error("AutoDeafen: send() incomplete (sent {} of {})", sent, len);
            break;
        }

        log::info("AutoDeafen: sent line '{}' to helper", line);
        success = true;
    } while (false);

    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
    WSACleanup();

    return success;
}

// Trigger deafen / undeafen (toggle in Discord)
bool triggerDeafen() {
    bool ok = sendHelperCommand("DEAFEN");
    if (ok) {
        log::info("AutoDeafen: triggerDeafen success");
    } else {
        log::error("AutoDeafen: triggerDeafen failed");
    }
    return ok;
}

// ---------------- MenuLayer hook (ready indicator) ----------------

class $modify(MyMenuLayer, MenuLayer) {
public:
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }

        log::info("AutoDeafen: MyMenuLayer::init (mod loaded)");

        auto winSize = CCDirector::get()->getWinSize();
        auto label   = CCLabelBMFont::create("AutoDeafen: Ready", "bigFont.fnt");
        if (label) {
            label->setPosition(winSize / 2);
            this->addChild(label);
        } else {
            log::warn("AutoDeafen: failed to create Ready label");
        }

        return true;
    }
};

// ---------------- PlayLayer hook ----------------

class $modify(MyPlayLayer, PlayLayer) {
public:
    struct Fields {
        bool hasDeafenedThisAttempt      = false; // we deafened at least once this attempt
        bool initializedRun              = false; // sampled initial percent yet
        bool isStartPosRun               = false; // this attempt started from > 0%
        bool wantDeafenOnNextStartPosRun = false; // re-deafen immediately on next StartPos respawn

        // Cached settings (updated each run)
        bool cachedActiveInPractice = false;
        bool cachedActiveInStartPos = false;
        bool cachedUndeafenOnDeath  = false;
    };

    void refreshSettings() {
        m_fields->cachedActiveInPractice = Mod::get()->getSettingValue<bool>("active-in-practice");
        m_fields->cachedActiveInStartPos = Mod::get()->getSettingValue<bool>("active-in-startpos");
        m_fields->cachedUndeafenOnDeath  = Mod::get()->getSettingValue<bool>("undeafen-on-death");
    }

    void resetAttemptState() {
        m_fields->hasDeafenedThisAttempt = false;
        m_fields->initializedRun         = false;
        m_fields->isStartPosRun          = false;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        // --- Classify this attempt on the first frame ---
        if (!m_fields->initializedRun) {
            // Refresh settings at the start of each attempt
            refreshSettings();

            float initialPercent = this->getCurrentPercent();
            m_fields->isStartPosRun = (initialPercent > 0.0f);
            m_fields->initializedRun = true;

            log::info(
                "AutoDeafen: run initialized - initialPercent={}, isStartPosRun={}",
                initialPercent,
                m_fields->isStartPosRun
            );

            // StartPos re-deafen on respawn:
            if (m_fields->cachedActiveInStartPos &&
                m_fields->isStartPosRun &&
                m_fields->wantDeafenOnNextStartPosRun) {

                log::info("AutoDeafen: re-deafening on StartPos respawn");
            if (triggerDeafen()) {
                // Mark that we've already deafened this attempt,
                // so we do NOT press again at the threshold.
                m_fields->hasDeafenedThisAttempt = true;
            }
            m_fields->wantDeafenOnNextStartPosRun = false;
                }
        }

        // --- Respect practice and startpos settings for new deafen actions ---

        // If disabled in practice and this is a practice run, do nothing.
        if (!m_fields->cachedActiveInPractice && this->m_isPracticeMode) {
            return;
        }

        // If disabled in startpos runs and this attempt started from >0%, do nothing.
        if (!m_fields->cachedActiveInStartPos && m_fields->isStartPosRun) {
            return;
        }

        // If we've already deafened this attempt (either at threshold or at respawn),
        // do nothing further.
        if (m_fields->hasDeafenedThisAttempt) {
            return;
        }

        // --- Percent-based deafen logic ---

        // Current level percent (0–100)
        float percent = this->getCurrentPercent();

        // Threshold from settings
        double threshold = Mod::get()->getSettingValue<double>("deafen-percent");
        float thresholdF = static_cast<float>(threshold);

        if (percent >= thresholdF) {
            log::info(
                "AutoDeafen: percent {} >= threshold {} – sending DEAFEN",
                percent,
                thresholdF
            );

            if (triggerDeafen()) {
                m_fields->hasDeafenedThisAttempt = true;
            } else {
                log::error("AutoDeafen: triggerDeafen failed (helper unreachable?)");
            }
        }
    }

    // ---- Exact undeafen-on-death hook ----
    // This is called when the player dies.
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        log::info("AutoDeafen: destroyPlayer (death) called");

        if (m_fields->cachedUndeafenOnDeath && m_fields->hasDeafenedThisAttempt) {
            if (m_fields->isStartPosRun && m_fields->cachedActiveInStartPos) {
                // StartPos: undeafen exactly on death and schedule re-deafen
                // on next StartPos respawn.
                log::info("AutoDeafen: StartPos run - undeafening on death and scheduling re-deafen on respawn");
                if (triggerDeafen()) {
                    m_fields->wantDeafenOnNextStartPosRun = true;
                }
            } else {
                // Normal run: just undeafen exactly on death.
                log::info("AutoDeafen: normal run - undeafening on death");
                triggerDeafen();
            }
        }

        // Always call the original behavior
        PlayLayer::destroyPlayer(player, object);
    }

    void resetLevel() {
        log::info("AutoDeafen: resetLevel called");

        // IMPORTANT: no toggling here anymore. This is purely state reset.
        // For StartPos runs, re-deafen will happen in postUpdate() at the
        // beginning of the next attempt if wantDeafenOnNextStartPosRun is set.

        resetAttemptState();

        PlayLayer::resetLevel();
    }

    void levelComplete() {
        log::info("AutoDeafen: levelComplete – resetting state");
        // On completion, we just clear all state. If you ever want different
        // behavior (e.g. auto-undeafen on completion) we can add a setting.
        resetAttemptState();
        m_fields->wantDeafenOnNextStartPosRun = false;

        PlayLayer::levelComplete();
    }
};
