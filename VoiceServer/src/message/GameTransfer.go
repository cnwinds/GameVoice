package message

import (
	"bytes"
	"encoding/binary"
	"net"
	"time"
	"github.com/golang/protobuf/proto"
)

// 兼容游戏包头
// len  	uint : 19
// split 	uint : 1
// id 		uint : 12
type GameHeader struct {
	header uint32
}

const GameHeaderSize = 4

func NewGameHeader(buf *bytes.Buffer) *GameHeader {
	h := new(GameHeader)
	if buf != nil {
		h.Read(buf)
	}
	return h
}
func (p *GameHeader) SetLen(len uint32) {
	p.header = p.header & 0xfff80000
	p.header = p.header | (uint32(len) & 0x0007ffff)
}
func (p *GameHeader) GetLen() uint32 {
	return uint32(p.header & 0x0007ffff)
}
func (p *GameHeader) SetCmd(cmd uint16) {
	p.header = p.header & 0x000fffff
	p.header = p.header | uint32(cmd&0x00000fff) << 20
}
func (p *GameHeader) GetCmd() uint16 {
	return uint16(p.header & 0xfff00000 >> 20)
}
func (p *GameHeader) Read(buf *bytes.Buffer) error {
	return binary.Read(buf, binary.LittleEndian, &p.header)
}
func (p *GameHeader) Write(buf *bytes.Buffer) error {
	return binary.Write(buf, binary.LittleEndian, p.header)
}

type GameTransfer struct {
	Conn   *net.TCPConn
}

func (this *GameTransfer) ReadPacket() (interface{}, *ControlMessage, error) {
	buf, err := this.read(GameHeaderSize)
	if err != nil {
		return nil, nil, err
	}
	header := NewGameHeader(bytes.NewBuffer(buf))
	body, err := this.read(int(header.GetLen()))
	msg := &ControlMessage{}
	err = proto.Unmarshal(body, msg)
	if err != nil {
		return nil, nil, err
	}
	return header, msg, nil

}
func (this *GameTransfer) SetReadTimeout(t time.Time) {
	this.Conn.SetReadDeadline(t)
}
func (this *GameTransfer) SetWriteTimeout(t time.Time) {
	this.Conn.SetWriteDeadline(t)
}
func (this *GameTransfer) read(count int) ([]byte, error) {
	buf := make([]byte, count)
	read := 0
	for read < count {
		len, e := this.Conn.Read(buf[read:])
		if e != nil {
			return nil, e
		}
		read = read + len
	}
	return buf, nil
}
func (this *GameTransfer) write(b []byte) error {
	total := len(b)
	for total != 0 {
		len, e := this.Conn.Write(b)
		if e != nil {
			return e
		}
		total = total - len
	}
	return nil
}
func (this *GameTransfer) WritePacket(cmd uint32, msg *ControlMessage) error {
	header := NewGameHeader(nil)
	header.SetCmd(uint16(cmd))
	body, _ := proto.Marshal(msg)
	header.SetLen(uint32(len(body)))
	buf := bytes.NewBuffer(make([]byte, 0, MSG_CONST_MAX_PACKET_SIZE))
	header.Write(buf)
	buf.Write(body)
	err := this.write(buf.Bytes())
	return err
}
func (this *GameTransfer) GetConnection() *net.TCPConn {
	return this.Conn
}
