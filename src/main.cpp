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
        closesocket(sock);
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
        bool hasDeafened = false;  // whether we deafened during this attempt
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        // Respect the "Active in practice mode" setting
        bool activeInPractice =
            Mod::get()->getSettingValue<bool>("active-in-practice");

        // If the mod is disabled in practice and this PlayLayer is in practice,
        // skip all deafen logic.
        if (!activeInPractice && this->m_isPracticeMode) {
            return;
        }

        // Current level percent (0–100)
        float percent = this->getCurrentPercent();

        // Threshold from settings
        double threshold = Mod::get()->getSettingValue<double>("deafen-percent");
        float thresholdF = static_cast<float>(threshold);

        // If already triggered this attempt, do nothing
        if (m_fields->hasDeafened) {
            return;
        }

        if (percent >= thresholdF) {
            log::info(
                "AutoDeafen: percent {} >= threshold {} – sending DEAFEN",
                percent,
                thresholdF
            );

            if (triggerDeafen()) {
                m_fields->hasDeafened = true;
            } else {
                log::error("AutoDeafen: triggerDeafen failed (helper unreachable?)");
            }
        }
    }

    void resetLevel() {
        log::info("AutoDeafen: resetLevel called");

        // Check setting: should we undeafen (toggle) on death / reset?
        bool undeafenOnDeath =
            Mod::get()->getSettingValue<bool>("undeafen-on-death");

        // If we had deafened during this attempt and the option is on,
        // send DEAFEN again to toggle back.
        if (undeafenOnDeath && m_fields->hasDeafened) {
            log::info("AutoDeafen: undeafen-on-death enabled and hasDeafened=true – sending DEAFEN again to undeafen");
            triggerDeafen();
        }

        // Reset state for the next attempt
        m_fields->hasDeafened = false;

        PlayLayer::resetLevel();
    }

    void levelComplete() {
        log::info("AutoDeafen: levelComplete – resetting hasDeafened");
        // For now we only undeafen on death (resetLevel); here we just clear state.
        m_fields->hasDeafened = false;
        PlayLayer::levelComplete();
    }
};
