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

    bool success = false;
    SOCKET sock  = INVALID_SOCKET;

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
            log::error(
                "AutoDeafen: send() incomplete (sent {} of {})",
                sent,
                len
            );
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
        // Per-attempt state
        bool hasDeafenedThisAttempt   = false; // we deafened at least once this attempt
        bool initializedRun           = false; // sampled initial percent yet
        bool isStartPosRun            = false; // this attempt started from > 0%

        // For “real death” detection
        float initialPercentForAttempt = 0.0f;
        bool  hasProgressedBeyondSpawn = false;

        // Anti-spam: only undeafen once per attempt
        bool deathHandledThisAttempt   = false;

        // Cached settings (updated each run)
        bool cachedActiveInPractice = false;
        bool cachedActiveInStartPos = false;
        bool cachedUndeafenOnDeath  = false;
    };

    void refreshSettings() {
        m_fields->cachedActiveInPractice =
            Mod::get()->getSettingValue<bool>("active-in-practice");
        m_fields->cachedActiveInStartPos =
            Mod::get()->getSettingValue<bool>("active-in-startpos");
        m_fields->cachedUndeafenOnDeath =
            Mod::get()->getSettingValue<bool>("undeafen-on-death");
    }

    void resetAttemptState() {
        m_fields->hasDeafenedThisAttempt   = false;
        m_fields->initializedRun           = false;
        m_fields->isStartPosRun            = false;
        m_fields->initialPercentForAttempt = 0.0f;
        m_fields->hasProgressedBeyondSpawn = false;
        m_fields->deathHandledThisAttempt  = false;
        // Nothing persists across attempts here; settings are re-read on first frame
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        // --- Classify this attempt on the first frame ---
        if (!m_fields->initializedRun) {
            // Refresh settings at the start of each attempt
            refreshSettings();

            float initialPercent = this->getCurrentPercent();
            m_fields->initialPercentForAttempt = initialPercent;
            m_fields->isStartPosRun            = (initialPercent > 0.0f);
            m_fields->initializedRun           = true;
            m_fields->hasProgressedBeyondSpawn = false;

            log::info(
                "AutoDeafen: run initialized - initialPercent={}, isStartPosRun={}",
                initialPercent,
                m_fields->isStartPosRun
            );
        }

        // --- Track whether we've actually moved beyond spawn percent ---
        if (m_fields->initializedRun) {
            float curPercent = this->getCurrentPercent();
            if (!m_fields->hasProgressedBeyondSpawn &&
                curPercent > m_fields->initialPercentForAttempt + 0.01f) {
                m_fields->hasProgressedBeyondSpawn = true;
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

        // If we've already deafened this attempt, do nothing further.
        if (m_fields->hasDeafenedThisAttempt) {
            return;
        }

        // --- Percent-based deafen logic ---

        // Current level percent (0–100)
        float percent = this->getCurrentPercent();

        // Threshold from settings
        double threshold =
            Mod::get()->getSettingValue<double>("deafen-percent");
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

    // ---- Undeafen-on-death hook ----
    // This is called when the player dies.
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        log::info(
            "AutoDeafen: destroyPlayer (death) called - hasDeafened={}, deathHandled={}, progressedBeyondSpawn={}",
            m_fields->hasDeafenedThisAttempt,
            m_fields->deathHandledThisAttempt,
            m_fields->hasProgressedBeyondSpawn
        );

        // Avoid running undeafen logic multiple times for the same attempt,
        // and only undeafen if:
        //  - the setting is enabled
        //  - we actually deafened earlier in this attempt
        //  - we progressed beyond the spawn percent (avoid spawn/teleport fake deaths)
        if (!m_fields->deathHandledThisAttempt &&
            m_fields->cachedUndeafenOnDeath &&
            m_fields->hasDeafenedThisAttempt &&
            m_fields->hasProgressedBeyondSpawn) {

            m_fields->deathHandledThisAttempt = true;

            log::info("AutoDeafen: undeafening on real death");
            triggerDeafen();
        }

        // Always call the original behavior
        PlayLayer::destroyPlayer(player, object);
    }

    void resetLevel() {
        log::info("AutoDeafen: resetLevel called");

        // Purely state reset for the next attempt.
        resetAttemptState();

        PlayLayer::resetLevel();
    }

    void levelComplete() {
        log::info("AutoDeafen: levelComplete – resetting state");

        // On completion, we just clear all state.
        resetAttemptState();

        PlayLayer::levelComplete();
    }
};
