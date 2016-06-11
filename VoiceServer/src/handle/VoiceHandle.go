package handle

import (
	"container/list"
	"message"
	"net"
	"object"
	"time"
	"utils"
)

const (
	VOICE_HEARTBEAT_INTERVAL_TIME   = 10                                // 心跳间隔(秒)
	VOICE_PLAYER_LOST_INTERVAL_TIME = VOICE_HEARTBEAT_INTERVAL_TIME * 4 // 丢失间隔(秒)
)

func HandleRequestConnect(conn message.UdpTransfer, addr *net.UDPAddr, msg *message.VoiceMessage, packet []byte) {
	req := msg.PlayerRequestConnect;
	player := AllGroups.FindPlayerByIndex(req.PlayerIndex, req.PlayerID)
	if player != nil {
		if player.GroupID != req.GroupID {
			utils.ErrInfo("request connect packet,  packet player groupid(%d) != memory player groupid(%d)", req.GroupID, player.GroupID)
			return
		}
		player.Addr = addr

		ack := &message.VoiceMessage{PlayerRequestConnectAck:&message.PlayerRequestConnectAck{}}
		ack.PlayerRequestConnectAck.Code = message.VOICE_ACK_VOICE_OK
		conn.WritePacket(uint32(message.VOICE_MSG_REQEUST_CONNECT), ack, addr)
	}
	return
}

func HandleVoiceData(conn message.UdpTransfer, addr *net.UDPAddr, msg *message.VoiceMessage, packet []byte) {
	req := msg.VoiceData;
	player := AllGroups.FindPlayerByIndex(req.PlayerIndex, req.PlayerID)
	if player != nil && player.IsSameAddr(addr) {
		g := AllGroups.FindGroup(player.GroupID)
		if g != nil {
			if player.State&object.PS_SPEAK != 0 {
				currentTime := time.Now().Unix()
				player.LastActiveTime = currentTime

				udp := conn.GetConnection()
				var delPlayerList *list.List
				g.ForeachPlayerList(func(k uint64, v *object.Player) {
					// 删除超时player
					if currentTime-v.LastActiveTime > VOICE_PLAYER_LOST_INTERVAL_TIME {
						if delPlayerList == nil {
							delPlayerList = list.New()
						}
						delPlayerList.PushBack(v)
					} else {
						if v.State != object.PS_MUTE {
							udp.WriteToUDP(packet, v.Addr)
						}
					}
				})
				// 删除超时player
				if delPlayerList != nil {
					for e := delPlayerList.Front(); e != nil; e = e.Next() {
						p, _ := e.Value.(*object.Player)
						AllGroups.DelPlayer(p.PlayerID)
					}
				}
			}
		}
	}
}

func HandleVoiceHeartbeat(conn message.UdpTransfer, addr *net.UDPAddr, msg *message.VoiceMessage, packet []byte) {
	req := msg.VoiceHeartbeat
	player := AllGroups.FindPlayerByIndex(req.PlayerIndex, req.PlayerID)
	if player != nil && player.IsSameAddr(addr) {
		player.LastActiveTime = time.Now().Unix()
	}
}
