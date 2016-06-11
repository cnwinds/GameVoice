package object

import (
	"bytes"
	"net"
	"time"
)

type PlayerState uint8

const (
	PS_WAIT_CONNECT PlayerState = 1
	PS_SPEAK                    = 2
	PS_LISTEN                   = 4
	PS_MUTE                     = 8
)

type Player struct {
	PlayerID       uint64
	GroupID        uint64
	State          PlayerState
	Addr           *net.UDPAddr
	PlayerIndex    uint32
	LastActiveTime int64
}

func NewPlayer(playerID uint64, groupID uint64) *Player {
	p := new(Player)
	p.PlayerID = playerID
	p.GroupID = groupID
	p.State = PS_WAIT_CONNECT
	p.LastActiveTime = time.Now().Unix()
	return p
}

func (p *Player) IsSameAddr(a *net.UDPAddr) bool {
	if a != nil && p.Addr != nil && p.Addr.Port == a.Port && bytes.Equal(p.Addr.IP, a.IP) {
		return true
	} else {
		return false
	}
}
