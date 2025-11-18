#define WIN32_LEAN_AND_MEAN
#include <winsock.h>

#include <Geode/Geode.hpp>
#include <Geode/Bindings.hpp>
#include <Geode/modify/MenuLayer.hpp>

using namespace geode::prelude;

// --- Networking helper ---

bool sendHelperCommand(const char* cmd) {
    WSADATA wsaData;
    // Winsock 1.x init
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
        log::error("WSAStartup failed");
        return false;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        log::error("socket() failed");
        WSACleanup();
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(44555);
    // 127.0.0.1
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    log::info("Attempting to connect to helper at 127.0.0.1:44555");

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        log::error("connect() to helper failed");
        closesocket(sock);
        WSACleanup();
        return false;
    }

    std::string msg = cmd;
    msg.push_back('\n');

    int len = static_cast<int>(msg.size());
    int sent = send(sock, msg.c_str(), len, 0);

    closesocket(sock);
    WSACleanup();

    if (sent != len) {
        log::error("send() incomplete: sent {} of {}", sent, len);
        return false;
    }

    log::info("Sent command '{}' to helper successfully", cmd);
    return true;
}

bool triggerDeafen() {
    bool ok = sendHelperCommand("DEAFEN");
    if (ok) {
        log::info("triggerDeafen: success");
    } else {
        log::error("triggerDeafen: failed");
    }
    return ok;
}

// --- MenuLayer hook ---

class $modify(MyMenuLayer, MenuLayer) {
public:
    bool init() {
        // Always call base first
        if (!MenuLayer::init()) {
            return false;
        }

        log::info("MyMenuLayer::init called");

        // Add a simple label in the middle so we visually know the mod loaded
        auto winSize = CCDirector::get()->getWinSize();
        auto label = CCLabelBMFont::create("AutoDeafen Test", "bigFont.fnt");
        if (label) {
            label->setPosition(winSize / 2);
            this->addChild(label);
        } else {
            log::warn("Failed to create test label");
        }

        // Try to contact helper and show result in a popup
        bool ok = triggerDeafen();
        if (ok) {
            FLAlertLayer::create(
                "AutoDeafen",
                "Sent DEAFEN to helper (success).",
                "OK"
            )->show();
        } else {
            FLAlertLayer::create(
                "AutoDeafen",
                "Failed to contact helper.\nIs gd_deafen_helper.py running?",
                "OK"
            )->show();
        }

        // Also log a debug message about children count
        log::debug(
            "MyMenuLayer::init hook: MenuLayer now has {} children.",
            this->getChildrenCount()
        );

        return true;
    }

    // Keep the button example if you want; it's optional for now
    void onMyButton(CCObject*) {
        FLAlertLayer::create("Geode", "Hello from my custom mod!", "OK")->show();
    }
};
