#define WIN32_LEAN_AND_MEAN
#include <winsock.h>

#include <Geode/Geode.hpp>
#include <Geode/Bindings.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/loader/Mod.hpp>

using namespace geode::prelude;

// sends commands to the python helper over tcp
bool sendHelperCommand(const char* cmd) {
    WSADATA wsaData;
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

bool triggerDeafen() {
    bool ok = sendHelperCommand("DEAFEN");
    if (ok) {
        log::info("AutoDeafen: triggerDeafen success");
    } else {
        log::error("AutoDeafen: triggerDeafen failed");
    }
    return ok;
}

class $modify(MyMenuLayer, MenuLayer) {
public:
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }

        log::info("AutoDeafen: MyMenuLayer::init (mod loaded)");

        // test connectivity to python helper
        bool helperConnected = sendHelperCommand("PING");
        
        if (!helperConnected) {
            log::error("AutoDeafen: failed to connect to helper on startup");
            
            // show error notification to user
            Notification::create(
                "AutoDeafen: Cannot connect to helper\n"
                "Make sure the Python helper is running on port 44555",
                NotificationIcon::Error,
                5.0f
            )->show();
        } else {
            log::info("AutoDeafen: helper connectivity test passed");
        }

        return true;
    }
};

class $modify(MyPlayLayer, PlayLayer) {
public:
    struct Fields {
        bool hasDeafenedThisAttempt   = false;
        bool initializedRun           = false;
        bool isStartPosRun            = false;

        // tracks whether player actually moved to detect real deaths vs spawn teleports
        float initialPercentForAttempt = 0.0f;
        bool  hasProgressedBeyondSpawn = false;
        bool  deathHandledThisAttempt  = false;

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
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        // initialize attempt state on first frame
        if (!m_fields->initializedRun) {
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

        if (m_fields->initializedRun) {
            float curPercent = this->getCurrentPercent();
            if (!m_fields->hasProgressedBeyondSpawn &&
                curPercent > m_fields->initialPercentForAttempt + 0.01f) {
                m_fields->hasProgressedBeyondSpawn = true;
            }
        }

        if (!m_fields->cachedActiveInPractice && this->m_isPracticeMode) {
            return;
        }

        if (!m_fields->cachedActiveInStartPos && m_fields->isStartPosRun) {
            return;
        }

        if (m_fields->hasDeafenedThisAttempt) {
            return;
        }

        float percent = this->getCurrentPercent();
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

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        log::info(
            "AutoDeafen: destroyPlayer (death) called - hasDeafened={}, deathHandled={}, progressedBeyondSpawn={}",
            m_fields->hasDeafenedThisAttempt,
            m_fields->deathHandledThisAttempt,
            m_fields->hasProgressedBeyondSpawn
        );

        // only undeafen on actual deaths not spawn teleports
        if (!m_fields->deathHandledThisAttempt &&
            m_fields->cachedUndeafenOnDeath &&
            m_fields->hasDeafenedThisAttempt &&
            m_fields->hasProgressedBeyondSpawn) {

            m_fields->deathHandledThisAttempt = true;

            log::info("AutoDeafen: undeafening on real death");
            triggerDeafen();
        }

        PlayLayer::destroyPlayer(player, object);
    }

    void resetLevel() {
        log::info("AutoDeafen: resetLevel called");
        resetAttemptState();
        PlayLayer::resetLevel();
    }

    void levelComplete() {
        log::info("AutoDeafen: levelComplete – resetting state");
        resetAttemptState();
        PlayLayer::levelComplete();
    }
};