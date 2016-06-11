#pragma once
#include <stdint.h>
#include <winsock.h>

#include "message.pb.h"
#include "VoiceModule.h"

#pragma pack(push)
#pragma pack(1)
struct UDP_HEADER {
    uint8_t     Cmd;
    uint16_t    Len;
};
#pragma pack(pop)
#define UDP_HEADER_LEN  sizeof(UDP_HEADER)

class VoiceServerProxy {
public:
    VoiceServerProxy();
    ~VoiceServerProxy();

    void Start(const char *pIP, int Port);
    void Stop();
    bool IsActive();

    void SendBaseInfo(uint32_t PlayerIndex, uint64_t PlayerID, uint64_t GroupID, bool CanSpeak);

    bool WaitRequestAck(uint8_t &Code);
    bool WaitVoiceData(uint64_t &SendPlayerID, uint32_t &FrameIndex, unsigned char *pBuf, int &Len);
    bool SendRequestConnect();
    bool SendVoiceData(uint32_t FrameIndex, const char *pBuf, int Len);
    bool SendHeartbeat();

private:
    static DWORD WINAPI ThreadProc1(_In_ LPVOID lpParameter);
    static DWORD WINAPI ThreadProc2(_In_ LPVOID lpParameter);

    void ProcessHeartbeat();
    void ProcessSocket();

private:
    bool ReadPacket(uint32_t &Cmd, message::VoiceMessage *pMsg);
    bool WritePacket(uint32_t Cmd, message::VoiceMessage *pMsg);

private:
    SOCKET      m_Socket;
    uint32_t    m_PlayerIndex;
    uint64_t    m_PlayerID;
    uint64_t    m_GroupID;
    bool        m_CanSpeak;

    VoiceModule m_VoiceModule;

    HANDLE      m_Thread1;
    HANDLE      m_Thread2;
};
