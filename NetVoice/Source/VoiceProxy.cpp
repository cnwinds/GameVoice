#include "stdafx.h"
#include "VoiceProxy.h"

VoiceServerProxy::VoiceServerProxy() {
    m_Socket = -1;
    m_PlayerIndex = 0;
    m_PlayerID = 0;
    m_GroupID = 0;
    m_VoiceModule.Initialize();
}

VoiceServerProxy::~VoiceServerProxy() {
    m_VoiceModule.Finalize();
}

void VoiceServerProxy::Start(const char *pIP, int Port) {
    m_Socket = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in to;
    to.sin_family = AF_INET;
    to.sin_port = htons(Port);
    to.sin_addr.s_addr = inet_addr(pIP);
    connect(m_Socket, (sockaddr*)&to, sizeof(to));

    m_Thread1 = CreateThread(NULL, 0, VoiceServerProxy::ThreadProc1, this, 0, NULL);

    WAVEFORMATEX wfx = { 0 };
    wfx.cbSize = 0;
    wfx.nChannels = CHANNELS;
    wfx.nSamplesPerSec = SAMPLES_PER_SEC;
    wfx.wBitsPerSample = BITS_PER_SAMPLE;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = (SAMPLES_PER_SEC * CHANNELS * BITS_PER_SAMPLE / 8);

    if (m_CanSpeak) {
        m_VoiceModule.StartCaputerMIC(wfx, this);
    }
    m_VoiceModule.StartPlayStream(wfx);
}

void VoiceServerProxy::Stop() {
    closesocket(m_Socket);
    m_Socket = -1;

    TerminateThread(m_Thread1, 0);
    TerminateThread(m_Thread2, 0);

    m_VoiceModule.StopCaptureMIC();
    m_VoiceModule.StopPlayStream();
}

bool VoiceServerProxy::IsActive() {
    return m_Socket != -1;
}

void VoiceServerProxy::SendBaseInfo(uint32_t PlayerIndex, uint64_t PlayerID, uint64_t GroupID, bool CanSpeak) {
    m_PlayerIndex = PlayerIndex;
    m_PlayerID = PlayerID;
    m_GroupID = GroupID;
    m_CanSpeak = CanSpeak;
}

bool VoiceServerProxy::SendRequestConnect() {
    message::VoiceMessage msg;
    message::PlayerRequestConnect *req = msg.mutable_playerrequestconnect();
    req->set_groupid(m_GroupID);
    req->set_playerid(m_PlayerID);
    req->set_playerindex(m_PlayerIndex);
    return WritePacket(message::REQEUST_CONNECT, &msg);
}

bool VoiceServerProxy::SendVoiceData(uint32_t FrameIndex, const char *pBuf, int Len) {
    message::VoiceMessage msg;
    message::VoiceData *h = msg.mutable_voicedata();
    h->set_playerid(m_PlayerID);
    h->set_playerindex(m_PlayerIndex);
    h->set_frameindex(FrameIndex);
    h->set_data(pBuf, Len);
    return WritePacket(message::VOICE_DATA, &msg);
}

bool VoiceServerProxy::SendHeartbeat() {
    message::VoiceMessage msg;
    message::VoiceHeartbeat *h = msg.mutable_voiceheartbeat();
    h->set_playerid(m_PlayerID);
    h->set_playerindex(m_PlayerIndex);
    return WritePacket(message::VOICE_HEARTBEAT, &msg);
}

bool VoiceServerProxy::WaitRequestAck(uint8_t &Code) {
    message::VoiceMessage msg;
    uint32_t cmd;
    if (ReadPacket(cmd, &msg)) {
        if (msg.has_playerrequestconnectack()) {
            Code = msg.playerrequestconnectack().code();
            return true;
        }
    }
    return false;
}

bool VoiceServerProxy::WaitVoiceData(uint64_t &SendPlayerID, uint32_t &FrameIndex, unsigned char *pBuf, int &Len) {
    message::VoiceMessage msg;
    uint32_t cmd;
    if (ReadPacket(cmd, &msg)) {
        if (msg.has_voicedata()) {
            SendPlayerID = msg.voicedata().playerid();
            FrameIndex = msg.voicedata().frameindex();
            std::string d = msg.voicedata().data();
            Len = min(d.length(), size_t(Len));
            memcpy(pBuf, d.c_str(), Len);
            return true;
        }
    }
    return false;
}

bool VoiceServerProxy::ReadPacket(uint32_t &Cmd, message::VoiceMessage *pMsg) {
    char buf[message::MAX_VOICE_PACKET_SIZE + UDP_HEADER_LEN];
    UDP_HEADER *header = (UDP_HEADER *)buf;
    int rlen = recv(m_Socket, (char *)buf, sizeof(buf), 0);
    if (rlen > 0) {
        Cmd = header->Cmd;
        int bodyLen = rlen - UDP_HEADER_LEN;
        if (header->Len == bodyLen) {
            pMsg->ParseFromArray(&buf[UDP_HEADER_LEN], bodyLen);
            return true;
        }
    }
    return false;
}

bool VoiceServerProxy::WritePacket(uint32_t Cmd, message::VoiceMessage *pMsg) {
    char buf[message::MAX_VOICE_PACKET_SIZE + UDP_HEADER_LEN];
    UDP_HEADER *header = (UDP_HEADER *)buf;
    header->Cmd = Cmd;
    header->Len = pMsg->ByteSize();
    if (pMsg->SerializeToArray(&buf[UDP_HEADER_LEN], message::MAX_VOICE_PACKET_SIZE)) {
        int slen = send(m_Socket, buf, header->Len + UDP_HEADER_LEN, 0);
        return slen == (header->Len + UDP_HEADER_LEN);
    }
    return false;
}

DWORD WINAPI VoiceServerProxy::ThreadProc1(_In_ LPVOID lpParameter) {
    VoiceServerProxy *This = (VoiceServerProxy *)lpParameter;
    This->ProcessSocket();
    return 0;
}

DWORD WINAPI VoiceServerProxy::ThreadProc2(_In_ LPVOID lpParameter) {
    VoiceServerProxy *This = (VoiceServerProxy *)lpParameter;
    This->ProcessHeartbeat();
    return 0;
}

void VoiceServerProxy::ProcessHeartbeat() {
    while (true) {
        if (IsActive()) {
            Sleep(10 * 1000);
            if (IsActive()) {
                SendHeartbeat();
            }
        }
        else {
            Sleep(1000);
        }
    }
}

void VoiceServerProxy::ProcessSocket() {
    if (!IsActive()) return;

    while (true) {
        if (SendRequestConnect()) {
            uint8_t code;
            // todo 增加超时退出
            if (WaitRequestAck(code)) {
                if (code == 0)
                    break;
                else {
                    LogError("ack failed. code = %d", code);
                    return;
                }
            }
            else {
                LogError("recv failed. code = %d", GetLastError());
                Sleep(1000);
            }
        }
        else {
            LogError("send failed. code = %d", GetLastError());
            return;
        }
    }

    m_Thread2 = CreateThread(NULL, 0, VoiceServerProxy::ThreadProc2, this, 0, NULL);

    while (true) {
        unsigned char recvbuf[VOICE_BUFFER];
        uint64_t sendplayerid;
        uint32_t frameindex;
        int len = sizeof(recvbuf);
        if (WaitVoiceData(sendplayerid, frameindex, recvbuf, len)) {
            m_VoiceModule.JitterPut(recvbuf, len, frameindex * FRAME_CYCLE);
        }
        else {
            // failed
            LogError("voice recv error, code = %d", GetLastError());
            return;
        }
    }
}
