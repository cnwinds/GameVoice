package handle

import (
	"config"
	"message"
	"net"
	"object"
	"utils"
)

func HandleHandshake(trans message.Transfer, msg *message.ControlMessage) {
	conn := trans.GetConnection()
	raddr, _ := conn.RemoteAddr().(*net.TCPAddr)
	ack := &message.ControlMessage{Res: &message.Response{}}
	if config.Verify(msg.Req.Handshake.ProductKey, raddr.IP.String()) {
		ack.Res.Code = message.ACK_OK
	} else {
		utils.ErrInfo("verify error(sn=%s,ip=%s)", msg.Req.Handshake.ProductKey, raddr.IP.String())
		ack.Res.Code = message.ACK_HANDSHAKE_FAILED
	}
	trans.WritePacket(uint32(message.MSG_HANDSHAKE_ACK), ack)
	if ack.Res.Code == message.ACK_HANDSHAKE_FAILED {
		conn.Close()
	}
}
func HandleEnterGroup(trans message.Transfer, msg *message.ControlMessage) {
	AllGroups.CreateGroup(msg.Req.EnterGroup.GroupId)
	player := AllGroups.CreatePlayer(msg.Req.EnterGroup.PlayerId, msg.Req.EnterGroup.GroupId)
	// ACK
	ack := &message.ControlMessage{Res: &message.Response{EnterGroupAck: &message.EnterGroupAck{}}}
	ega := ack.Res.EnterGroupAck
	if player != nil {
		player.State = object.PS_LISTEN
		if msg.Req.EnterGroup.CanSpeak != 0 {
			player.State |= object.PS_SPEAK
		}

		ack.Res.Code = message.ACK_OK
		ega.PlayerIndex = player.PlayerIndex
		ega.PlayerId = player.PlayerID
		ega.GroupId = player.GroupID

	} else {
		ack.Res.Code = message.ACK_ADD_PLAYER_FAILED
	}
	trans.WritePacket(uint32(message.MSG_ENTER_GROUP_ACK), ack)
	// todo send NotifyAddPlayer
}

func HandleLeaveGroup(trans message.Transfer, msg *message.ControlMessage) {
	// todo AllGroups.DelGroup(msg.Req.LeaveGroup.GroupId)
}
func HandleAdjustPlayerAttr(trans message.Transfer, msg *message.ControlMessage) {
	player := AllGroups.FindPlayer(msg.Req.AdjustPlayerAttr.PlayerId)
	player.State = object.PlayerState(msg.Req.AdjustPlayerAttr.CanSpeak)
}
func HandleAdjustGroupAttr(trans message.Transfer, msg *message.ControlMessage) {
}
