#include "stdafx.h"
#include "GameProxy.h"

GameServerProxy::GameServerProxy() : m_CmdList() {
    m_pEvent = NULL;
    m_GroupID = 0;
    m_PlayerID = 0;
    m_Status = S_NOTHING;
    m_Port = 0;
    m_Thread1 = NULL;
    m_Thread2 = NULL;
}

void GameServerProxy::StartConnect(const char *pIP, int Port, const string &ProductKey) {
    m_IP = pIP;
    m_Port = Port;
    m_Productkey = ProductKey;
    m_Thread1 = CreateThread(NULL, 0, ThreadProc1, this, 0, NULL);
    m_Thread2 = CreateThread(NULL, 0, ThreadProc2, this, 0, NULL);
}

void GameServerProxy::StopConnect() {
    m_Status = S_STOP;
    TerminateThread(m_Thread1, 0);
    TerminateThread(m_Thread2, 0);
}

void GameServerProxy::SetEvent(IGameServerEvent *pEvent) {
    m_pEvent = pEvent;
}

void GameServerProxy::SetBaseInfo(uint64_t GroupID, uint64_t PlayerID) {
    m_GroupID = GroupID;
    m_PlayerID = PlayerID;
}

bool GameServerProxy::IsReady() {
    return m_Status == S_CONNECTED;
}

bool GameServerProxy::IsClose() {
    return m_Status != S_CONNECTING && m_Status != S_CONNECTED;
}

void GameServerProxy::EnterGroup(uint64_t GroupID, uint64_t PlayerID, bool CanSpeak) {
    _cmd c;
    c.Cmd = CMD_ENTER_GROUP;
    c.Param1 = GroupID;
    c.Param2 = PlayerID;
    c.Param3 = CanSpeak;
    m_CmdList.Put(c);
    m_CanSpeak = CanSpeak;
}

void GameServerProxy::LeaveGroup(uint64_t GroupID, uint64_t PlayerID) {
    _cmd c;
    c.Cmd = CMD_LEAVE_GROUP;
    c.Param1 = GroupID;
    c.Param2 = PlayerID;
    m_CmdList.Put(c);
}

void GameServerProxy::ProcessCmdChannel() {
    while (m_Status != S_STOP) {
        if (m_Status == S_CONNECTED) {
            _cmd c;
            while (m_CmdList.Get(c)) {
                switch (c.Cmd) {
                case CMD_ENTER_GROUP:
                    SendEnterGroup(c.Param1, c.Param2, c.Param3);
                    break;
                case CMD_LEAVE_GROUP:
                    SendLeaveGroup(c.Param1, c.Param2);
                    break;
                }
            }
        }
        else {
            Sleep(50);
        }
    }
}

void GameServerProxy::ProcessSocket() {
    m_Status = GameServerProxy::S_CONNECTING;
    int handshakecount = 0;
    while (m_Status != S_STOP) {
        m_Status = GameServerProxy::S_CONNECTING;
        m_Socket = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in to;
        to.sin_family = AF_INET;
        to.sin_port = htons(m_Port);
        to.sin_addr.s_addr = inet_addr(m_IP.c_str());
        if (connect(m_Socket, (sockaddr*)&to, sizeof(to)) != 0) {
            m_Status = GameServerProxy::S_DISCONNECTED;
            Sleep(1000);
            continue;
        }

        bool reconnect = true;
        SendHandshake(m_Productkey);
        uint32_t cmd;
        message::ControlMessage msg;
        if (ReadPacket(cmd, &msg)) {
            if ((cmd == message::HANDSHAKE_ACK) && msg.has_res()) {
                if (msg.res().code() != message::OK) {
                    LogError("auth failed. response code=%d", msg.res().code());
                    if (m_pEvent) {
                        m_pEvent->OnHandshake(msg.res().code());
                    }
                    return;
                }
                reconnect = false;
            }
        }
        if (reconnect) {
            closesocket(m_Socket);
            Sleep(1000);
            continue;
        }

        if ((handshakecount != 0)) {
            if (m_pEvent) m_pEvent->OnReconnect();
            if (m_GroupID != 0) {
                EnterGroup(m_GroupID, m_PlayerID, m_CanSpeak);
            }
        }
        handshakecount++;

        if (m_pEvent) {
            m_pEvent->OnHandshake(message::OK);
        }

        m_Status = GameServerProxy::S_CONNECTED;
        while (m_Status != S_STOP) {
            uint32_t cmd;
            message::ControlMessage msg;
            if (ReadPacket(cmd, &msg)) {
                switch (cmd) {
                case message::ENTER_GROUP_ACK:
                {
                    message::EnterGroupAck p = msg.res().entergroupack();
                    if (msg.res().code() == message::VOICE_OK) {
                        m_PlayerID = p.playerid();
                        m_GroupID = p.groupid();
                        m_PlayerIndex = p.playerindex();
                        // Æô¶¯ÓïÒô
                        m_VoiceProxy.SendBaseInfo(m_PlayerIndex, m_PlayerID, m_GroupID, m_CanSpeak);
                        m_VoiceProxy.Start(m_IP.c_str(), m_Port);
                        if (m_pEvent) {
                            m_pEvent->OnEnterGroup(p.groupid(), p.playerid(), p.playerindex());
                        }
                    }

                    break;
                }
                case message::NOTIFY_LEAVE_GROUP:
                {
                    message::NotifyLeaveGroup p = msg.notify().notifyleavegroup();
                    m_PlayerID = 0;
                    m_GroupID = 0;
                    m_CanSpeak = false;
                    // Í£Ö¹ÓïÒô
                    m_VoiceProxy.Stop();
                    if (m_pEvent) {
                        m_pEvent->OnLeaveGroup(p.groupid(), p.playerid());
                    }
                    break;
                }
                case message::NOTIFY_ENTER_GROUP:
                {
                    message::NotifyEnterGroup p = msg.notify().notifyentergroup();
                    if (m_pEvent) {
                        m_pEvent->OnNotifyEnterGroup(p.groupid(), p.playerid(), p.canspeak());
                    }
                    break;
                }
                }
            }
            else {
                // ¶ÏÏßÖØÁ¬
                m_VoiceProxy.Stop();
                if (m_pEvent) {
                    m_pEvent->OnDisconnect();
                }
                Sleep(5000);
                break;
            }
        }
    }
}

DWORD WINAPI GameServerProxy::ThreadProc1(_In_ LPVOID lpParameter) {
    GameServerProxy *This = (GameServerProxy *)lpParameter;
    This->ProcessSocket();
    return 0;
}

DWORD WINAPI GameServerProxy::ThreadProc2(_In_ LPVOID lpParameter) {
    GameServerProxy *This = (GameServerProxy *)lpParameter;
    This->ProcessCmdChannel();
    return 0;
}

bool GameServerProxy::ReadPacket(uint32_t &Cmd, message::ControlMessage *pMsg) {
    char buf[message::MAX_PACKET_SIZE + CTRL_HEADER_LEN];
    CTRL_HEADER *header = (CTRL_HEADER *)buf;
    int rlen = recv(m_Socket, buf, CTRL_HEADER_LEN, 0);
    if (rlen > 0) {
        Cmd = header->Cmd;
        int bodyLen = header->Len;
        int rlen = recv(m_Socket, &buf[CTRL_HEADER_LEN], bodyLen, 0);
        if (rlen > 0) {
            pMsg->ParseFromArray(&buf[CTRL_HEADER_LEN], bodyLen);
            return true;
        }
    }
    else {
        LogError("recv error, code=%d", GetLastError());
    }
    return false;
}

bool GameServerProxy::WritePacket(uint32_t Cmd, message::ControlMessage *pMsg) {
    char buf[message::MAX_PACKET_SIZE + CTRL_HEADER_LEN];
    CTRL_HEADER *header = (CTRL_HEADER *)buf;
    header->Cmd = Cmd;
    header->Len = pMsg->ByteSize();
    if (pMsg->SerializeToArray(&buf[CTRL_HEADER_LEN], sizeof(buf))) {
        int slen = send(m_Socket, buf, header->Len + CTRL_HEADER_LEN, 0);
        return slen == (header->Len + CTRL_HEADER_LEN);
    }
    return false;
}
bool GameServerProxy::SendHandshake(const string &ProductKey) {
    message::ControlMessage msg;
    message::Handshake *p = msg.mutable_req()->mutable_handshake();
    p->set_productkey(ProductKey);
    return WritePacket(message::HANDSHAKE, &msg);
}

bool GameServerProxy::SendEnterGroup(uint64_t GroupID, uint64_t PlayerID, bool CanSpeak) {
    message::ControlMessage msg;
    message::EnterGroup *p = msg.mutable_req()->mutable_entergroup();
    p->set_groupid(GroupID);
    p->set_playerid(PlayerID);
    p->set_canspeak(CanSpeak);
    return WritePacket(message::ENTER_GROUP, &msg);
}

bool GameServerProxy::SendLeaveGroup(uint64_t GroupID, uint64_t PlayerID) {
    message::ControlMessage msg;
    message::LeaveGroup *p = msg.mutable_req()->mutable_leavegroup();
    p->set_groupid(GroupID);
    p->set_playerid(PlayerID);
    return WritePacket(message::LEAVE_GROUP, &msg);
}
