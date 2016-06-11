#include "stdafx.h"

#include "GameProxy.h"
#include "Utils.h"

#define VOICE_PORT          9100
//#define VOICE_IP            "127.0.0.1"
#define VOICE_IP            "183.136.130.151"

GameServerProxy gsp;

class Callback : IGameServerEvent {
public:
    virtual void OnNotifyEnterGroup(uint64_t groupid, uint64_t playerid, uint8_t canspeak) {
        LogInfo("OnNotifyEnterGroup - groupid=%lld, playerid=%lld, canspeak=%d", groupid, playerid, canspeak);
    }
    virtual void OnNotifyLeaveGroup(uint64_t groupid, uint64_t playerid) {
        LogInfo("OnNotifyLeaveGroup - groupid=%lld, playerid=%lld", groupid, playerid);
    }
    virtual void OnPlayerAttrChange(uint64_t groupid, uint64_t playerid, int attr) {
        LogInfo("OnPlayerAttrChange - groupid=%lld, playerid=%lld, attr=%d", groupid, playerid, attr);
    }
    virtual void OnEnterGroup(uint64_t groupid, uint64_t playerid, uint32_t playerindex) {
        LogInfo("OnEnterGroup - groupid=%lld, playerid=%lld, playerindex=%d", groupid, playerid, playerindex);
    }
    virtual void OnLeaveGroup(uint64_t groupid, uint64_t playerid) {
        LogInfo("OnLeaveGroup - groupid=%lld, playerid=%lld", groupid, playerid);
    }
    virtual void OnReconnect() {
        LogInfo("OnReconnect");
    }
    virtual void OnDisconnect() {
        LogInfo("OnDisconnect");
    }
    virtual void OnHandshake(uint32_t code) {
        LogInfo("OnHandshake - code=%d", code);
    }
};

int main(int _argc, const char **_argv) {
    WSADATA data;
    WSAStartup(0x202, &data);

    IGameServerEvent *cb = (IGameServerEvent *)(new Callback());

    uint64_t groupid = 1;
    uint64_t playerid = 1;
    bool canspeak = true;

    if (_argc > 3) {
        groupid = atoi(_argv[1]);
        playerid = atoi(_argv[2]);
        canspeak = atoi(_argv[3]);
    }

    gsp.SetEvent(cb);
    gsp.StartConnect(VOICE_IP, VOICE_PORT, "abcdefg");
    gsp.EnterGroup(groupid, playerid, canspeak);

    while (true)
    {
        Sleep(5000);
    }

    return 0;
}