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

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        log::error("AutoDeafen: socket() failed");
        WSACleanup();
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(44555);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    log::info("AutoDeafen: connecting to helper at 127.0.0.1:44555");

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        log::error("AutoDeafen: connect() to helper failed");
        WSACleanup();
        return false;
    }

    // Send just the command and a newline, e.g. "DEAFEN\n"
    std::string line = cmd;
    line.push_back('\n');

    int len  = static_cast<int>(line.size());
    int sent = send(sock, line.c_str(), len, 0);

    closesocket(sock);
    WSACleanup();

    if (sent != len) {
        log::error("AutoDeafen: send() incomplete (sent {} of {})", sent, len);
        return false;
    }

    log::info("AutoDeafen: sent line '{}' to helper", line);
    return true;
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
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        // --- Classify this attempt on the first frame ---
        if (!m_fields->initializedRun) {
            float initialPercent = this->getCurrentPercent();
            m_fields->isStartPosRun = (initialPercent > 0.0f);
            m_fields->initializedRun = true;

            log::info(
                "AutoDeafen: run initialized - initialPercent={}, isStartPosRun={}",
                initialPercent,
                m_fields->isStartPosRun
            );

            // StartPos re-deafen on respawn:
            bool activeInStartPos =
                Mod::get()->getSettingValue<bool>("active-in-startpos");

            if (activeInStartPos &&
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

        bool activeInPractice =
            Mod::get()->getSettingValue<bool>("active-in-practice");
        bool activeInStartPos =
            Mod::get()->getSettingValue<bool>("active-in-startpos");

        // If disabled in practice and this is a practice run, do nothing.
        if (!activeInPractice && this->m_isPracticeMode) {
            return;
        }

        // If disabled in startpos runs and this attempt started from >0%, do nothing.
        if (!activeInStartPos && m_fields->isStartPosRun) {
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

    void resetLevel() {
        log::info("AutoDeafen: resetLevel called");

        bool undeafenOnDeath =
            Mod::get()->getSettingValue<bool>("undeafen-on-death");
        bool activeInStartPos =
            Mod::get()->getSettingValue<bool>("active-in-startpos");

        // If we deafened this attempt and undeafen-on-death is enabled:
        if (undeafenOnDeath && m_fields->hasDeafenedThisAttempt) {
            if (m_fields->isStartPosRun && activeInStartPos) {
                // StartPos: undeafen at reset and schedule re-deafen
                // on next StartPos respawn.
                log::info("AutoDeafen: StartPos run - undeafening on reset and scheduling re-deafen on respawn");
                if (triggerDeafen()) {
                    m_fields->wantDeafenOnNextStartPosRun = true;
                }
            } else {
                // Normal run: just undeafen once at reset.
                log::info("AutoDeafen: normal run - undeafening on reset");
                triggerDeafen();
            }
        }

        // Reset per-attempt state for the next run.
        // NOTE: we intentionally DO NOT clear wantDeafenOnNextStartPosRun here;
        // it is consumed at the beginning of the next StartPos run in postUpdate.
        m_fields->hasDeafenedThisAttempt = false;
        m_fields->initializedRun         = false;
        m_fields->isStartPosRun          = false;

        PlayLayer::resetLevel();
    }

    void levelComplete() {
        log::info("AutoDeafen: levelComplete – resetting state");
        // On completion, we just clear all state. If you ever want different
        // behavior (e.g. auto-undeafen on completion) we can add a setting.
        m_fields->hasDeafenedThisAttempt      = false;
        m_fields->initializedRun              = false;
        m_fields->isStartPosRun               = false;
        m_fields->wantDeafenOnNextStartPosRun = false;

        PlayLayer::levelComplete();
    }
};
