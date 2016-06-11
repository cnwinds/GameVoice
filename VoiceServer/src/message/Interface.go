package message

import (
	"time"
	"net"
)

type HandleFunction func(trans Transfer, msg *ControlMessage)

type Transfer interface {
	ReadPacket() (header interface{}, msg *ControlMessage, err error)
	WritePacket(cmd uint32, msg *ControlMessage) error

	SetReadTimeout(t time.Time)
	SetWriteTimeout(t time.Time)
	GetConnection() *net.TCPConn
}

type HandleUDPFunction func(trans UdpTransfer, peer *net.UDPAddr, msg *VoiceMessage, packet []byte)

type UdpTransfer interface {
	ReadPacket() (peer *net.UDPAddr, header interface{}, msg *VoiceMessage, packet []byte, err error)
	WritePacket(cmd uint32, msg *VoiceMessage, peer *net.UDPAddr) error
	GetConnection() *net.UDPConn
}