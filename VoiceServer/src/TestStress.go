package main

import (
	"../../../../../../../src/message"
	"bytes"
	"fmt"
	"math/rand"
	"net"
	"object"
	"time"
	"utils"
)

type testPlayer struct {
	Conn            *net.UDPConn
	Player          *object.Player
	LastFrameIndex  uint32
	SendFrameIndex  uint32
	MissPacketCount uint32
	RecvPacketCount uint32
	LastMissCount   uint32
	LastRecvCount   uint32
	Data            chan []byte
	RecvCount       chan uint32
	MissCount       chan uint32
}

type testPlayers struct {
	List map[uint64]*testPlayer
}

var AllTestPlayer *testPlayers

func (this *testPlayers) NewTestPlayer(groupid uint64, playerid uint64) *testPlayer {
	p := new(testPlayer)
	this.List[playerid] = p
	p.Player = object.NewPlayer(playerid, groupid)
	p.Data = make(chan []byte, 10)
	p.RecvCount = make(chan uint32, 10)
	p.MissCount = make(chan uint32, 10)
	return p
}
func (this *testPlayers) FindTestPlayer(playerid uint64) *testPlayer {
	v, _ := this.List[playerid]
	return v
}

var (
	conn *net.UDPConn
	addr *net.UDPAddr
)

const (
	testPlayerCount int = 100
)

func ReadBody(gs message.Transfer) (uint16, []byte) {
	h, e := gs.ReadHeader()
	utils.CheckError(e, "read header")
	header, _ := h.(*message.GameHeader)
	body, err := gs.ReadBody()
	utils.CheckError(err, "read body")
	return header.GetCmd(), body
}

func GameConnect() {
	addr, _ := net.ResolveTCPAddr("tcp", "10.211.55.11:9000")
	conn, err := net.DialTCP("tcp", nil, addr)
	utils.CheckError(err, "tcp dial error")
	defer conn.Close()

	gs := message.GameTransfer{Conn: conn}

	p := message.NewHandshake(nil)
	p.ProductKey = "123456"
	pbuf := message.GetBytes(p.Write)
	gs.WritePacket(message.GAME_CMD_HANDSHAKE, pbuf)

	cmd, body := ReadBody(&gs)
	if cmd == message.GAME_CMD_HANDSHAKE_ACK {
		ack := message.NewHandshakeAck(bytes.NewBuffer(body))
		if ack.Code != message.GAME_ACK_OK {
			utils.ErrInfo("handshank ack err=%d", ack.Code)
		}
	}

	for i := 0; i < testPlayerCount; i++ {
		player := AllTestPlayer.NewTestPlayer(1, uint64(i+100000))
		p := message.NewAddPlayer(nil)
		p.GroupID = player.Player.GroupID
		p.PlayerID = player.Player.PlayerID
		if i == 0 {
			player.Player.State = object.PS_SPEAK | object.PS_LISTEN
		} else {
			player.Player.State = object.PS_LISTEN
		}
		p.CanSpeak = uint8(player.Player.State)
		pbuf := message.GetBytes(p.Write)
		gs.WritePacket(message.GAME_CMD_ADD_PLAYER, pbuf)
	}

	for i := 0; i < testPlayerCount; i++ {
		cmd, body := ReadBody(&gs)
		ack := message.NewAddPlayerAck(bytes.NewBuffer(body))
		if cmd == message.GAME_CMD_ADD_PLAYER_ACK {
			if ack.Code != message.GAME_ACK_OK {
				utils.ErrInfo("handshank ack err=%d", ack.Code)
			}
			player := AllTestPlayer.FindTestPlayer(ack.PlayerID)
			if player != nil {
				player.Player.PlayerIndex = ack.PlayerIndex
			}
			go handleVoice(player)
		}
	}

}

func main() {
	AllTestPlayer = new(testPlayers)
	AllTestPlayer.List = make(map[uint64]*testPlayer)

	GameConnect()

	for {
		var first bool = true
		for _, v := range AllTestPlayer.List {
			rc := <-v.RecvCount
			// mc := <-v.MissCount
			if first {
				total := uint32(1000 / 40 * 5)
				utils.LogInfo("total=%d, playerid=%d, recvcount=%d, miss=%f%%",
					len(AllTestPlayer.List), v.Player.PlayerID, rc, float32(total-rc)*100/float32(total))
				first = false
			}
		}
	}
}

func handleVoice(p *testPlayer) {
	saddr := fmt.Sprintf("10.211.55.11:%d", 9000)
	addr, _ = net.ResolveUDPAddr("udp", saddr)
	conn, err := net.DialUDP("udp", nil, addr)
	utils.CheckError(err, "dail udp")
	defer conn.Close()

	vt := &message.VoiceTransfer{UDP: conn}
	timeout := time.Tick(time.Second * 5)
	connack := time.After(time.Microsecond)
	go func() {
		for {
			buf := make([]byte, message.MAX_VOICE_PACKET_SIZE)
			len, err := conn.Read(buf)
			if err == nil {
				p.Data <- buf[:len]

			} else {
				utils.LogInfo("%s", err.Error())
				p.Data <- buf[0:0]
				return
			}
		}
	}()

	var speak <-chan time.Time
	if p.Player.State&object.PS_SPEAK != 0 {
		speak = time.Tick(time.Millisecond * 40)
	}

	var heartbeat <-chan time.Time
	heartbeat = time.Tick(time.Second * 10)

	for {
		select {
		case <-connack:
			pkt := message.NewPlayRequestConnect(nil)

			pkt.GroupID = p.Player.GroupID
			pkt.PlayerID = p.Player.PlayerID
			pkt.PlayerIndex = p.Player.PlayerIndex
			buf := message.GetBytes(pkt.Write)
			vt.WritePacket(message.VOICE_CMD_REQUEST_CONNECT, buf)
			connack = time.After(time.Microsecond * 200)
		case buf := <-p.Data:
			bbuf := bytes.NewBuffer(buf)
			h := message.NewVoiceHeader(bbuf)
			switch h.Cmd {
			case message.VOICE_CMD_REQUEST_CONNECT_ACK:
				pkt := message.NewPlayerRequestConnectAck(bbuf)
				if pkt.Code != message.VOICE_ACK_OK {
					utils.LogInfo("NewPlayerRequestConnectAck result=%d", pkt.Code)
					return
				}
				connack = nil
			case message.VOICE_CMD_VOICE_DATA:
				pkt := message.NewVoiceData(bbuf)
				p.RecvPacketCount += 1
				if p.LastFrameIndex == 0 {
					p.LastFrameIndex = pkt.FrameIndex
				} else {
					if p.LastFrameIndex+1 != pkt.FrameIndex {
						p.MissPacketCount += 1
					}
					p.LastFrameIndex = pkt.FrameIndex
				}
			}
		case <-timeout:
			// p.MissCount <- p.MissPacketCount - p.LastMissCount
			p.LastMissCount = p.MissPacketCount
			p.RecvCount <- p.RecvPacketCount - p.LastRecvCount
			p.LastRecvCount = p.RecvPacketCount
		case <-speak:
			pkt := message.NewVoiceData(nil)
			p.SendFrameIndex++
			pkt.FrameIndex = p.SendFrameIndex
			pkt.PlayerID = p.Player.PlayerID
			pkt.PlayerIndex = p.Player.PlayerIndex
			l := rand.Uint32()%90 + rand.Uint32()%30
			pkt.Data = make([]byte, l)
			buf := message.GetBytes(pkt.Write)
			vt.WritePacket(message.VOICE_CMD_VOICE_DATA, buf)
		case <-heartbeat:
			pkt := message.NewVoiceHeartbeat(nil)
			pkt.PlayerID = p.Player.PlayerID
			pkt.PlayerIndex = p.Player.PlayerIndex
			buf := message.GetBytes(pkt.Write)
			vt.WritePacket(message.VOICE_CMD_HEARTBEAT, buf)
		}
	}
}
