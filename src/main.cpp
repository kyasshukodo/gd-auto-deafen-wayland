#define WIN32_LEAN_AND_MEAN
#include <winsock.h>

#include <Geode/Geode.hpp>
#include <Geode/Bindings.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/loader/Mod.hpp>  // for Mod::get()->getSettingValue

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

    std::string msg = cmd;
    msg.push_back('\n');

    int len  = static_cast<int>(msg.size());
    int sent = send(sock, msg.c_str(), len, 0);

    closesocket(sock);
    WSACleanup();

    if (sent != len) {
        log::error("AutoDeafen: send() incomplete (sent {} of {})", sent, len);
        return false;
    }

    log::info("AutoDeafen: sent '{}' to helper", cmd);
    return true;
}

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

// ---------------- PlayLayer hook (percent-based deafen with setting) ----------------

class $modify(MyPlayLayer, PlayLayer) {
public:
    struct Fields {
        bool hasDeafened = false;  // once per attempt
    };

    // Called every frame after the base PlayLayer update
    void postUpdate(float dt) {
        // Call original implementation first
        PlayLayer::postUpdate(dt);

        // Current level percent (0–100)
        float percent = this->getCurrentPercent();

        // Read current threshold from settings (float setting, we read as double)
        double threshold = Mod::get()->getSettingValue<double>("deafen-percent");
        float thresholdF = static_cast<float>(threshold);

        // If already triggered this attempt, do nothing
        if (m_fields->hasDeafened) {
            return;
        }

        // Trigger once when crossing threshold
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

    // Reset per-attempt state when the level is reset
    void resetLevel() {
        log::info("AutoDeafen: resetLevel – resetting hasDeafened");
        m_fields->hasDeafened = false;
        PlayLayer::resetLevel();
    }

    // Also reset when the level completes
    void levelComplete() {
        log::info("AutoDeafen: levelComplete – resetting hasDeafened");
        m_fields->hasDeafened = false;
        PlayLayer::levelComplete();
    }
};
