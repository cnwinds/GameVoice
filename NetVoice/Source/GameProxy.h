#pragma once
#include <stdint.h>
#include <winsock.h>
#include <string>
#include <vector>

#include "message.pb.h"
#include "Channel.h"
#include "VoiceProxy.h"

using namespace std;

#pragma pack(push)
#pragma pack(1)
struct CTRL_HEADER {
    UINT   Len : 19;
    UINT   Split : 1;
    UINT   Cmd : 12;
    CTRL_HEADER() { Len = 0; Split = 0; Cmd = 0; }
};
#pragma pack(pop)
#define CTRL_HEADER_LEN  sizeof(CTRL_HEADER)

class IGameServerEvent {
public:
    virtual ~IGameServerEvent() {};

public:
    virtual void OnNotifyEnterGroup(uint64_t GroupID, uint64_t PlayerID, uint8_t CanSpeak) = 0;
    virtual void OnNotifyLeaveGroup(uint64_t GroupID, uint64_t PlayerID) = 0;
    virtual void OnPlayerAttrChange(uint64_t GroupID, uint64_t PlayerID, int Attr) = 0;
    virtual void OnEnterGroup(uint64_t GroupID, uint64_t PlayerID, uint32_t PlayerIndex) = 0;
    virtual void OnLeaveGroup(uint64_t GroupID, uint64_t PlayerID) = 0;

    virtual void OnReconnect() = 0;
    virtual void OnHandshake(uint32_t Code) = 0;
    virtual void OnDisconnect() = 0;
};

class GameServerProxy {
public:
    GameServerProxy();

    void StartConnect(const char *pIP, int Port, const string &ProductKey);
    void StopConnect();
    void SetEvent(IGameServerEvent *pEvent);
    void SetBaseInfo(uint64_t GroupID, uint64_t PlayerID);
    bool IsReady();
    bool IsClose();

    void EnterGroup(uint64_t GroupID, uint64_t PlayerID, bool CanSpeak);
    void LeaveGroup(uint64_t GroupID, uint64_t PlayerID);

private:
    static DWORD WINAPI ThreadProc1(_In_ LPVOID lpParameter);
    static DWORD WINAPI ThreadProc2(_In_ LPVOID lpParameter);

private:
    void ProcessCmdChannel();
    void ProcessSocket();

private:
    bool SendHandshake(const string &ProductKey);
    bool SendEnterGroup(uint64_t GroupID, uint64_t PlayerID, bool CanSpeak);
    bool SendLeaveGroup(uint64_t GroupID, uint64_t PlayerID);

private:
    bool ReadPacket(uint32_t &Cmd, message::ControlMessage *pMsg);
    bool WritePacket(uint32_t Cmd, message::ControlMessage *pMsg);

private:
    string              m_IP;
    int                 m_Port;
    string              m_Productkey;

    IGameServerEvent    *m_pEvent;
    uint64_t            m_GroupID;
    uint64_t            m_PlayerID;
    uint32_t            m_PlayerIndex;
    bool                m_CanSpeak;

    enum { CMD_ENTER_GROUP, CMD_LEAVE_GROUP };
    struct _cmd {
        int         Cmd;
        uint64_t    Param1;
        uint64_t    Param2;
        bool        Param3;
    };
    Channel<_cmd>   m_CmdList;

    enum { S_NOTHING, S_CONNECTING, S_CONNECTED, S_DISCONNECTED, S_STOP };

    int             m_Status;
    SOCKET          m_Socket;

    HANDLE          m_Thread1;
    HANDLE          m_Thread2;

    VoiceServerProxy    m_VoiceProxy;
};
