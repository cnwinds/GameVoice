package message

import (
	"bytes"
	"net"
	"encoding/binary"
	"github.com/golang/protobuf/proto"
)

type VoiceHeader struct {
	Cmd    uint8
	Length uint16
}

const VoiceHeaderSize = 3

func NewVoiceHeader(buf *bytes.Buffer) *VoiceHeader {
	header := new(VoiceHeader)
	if buf != nil {
		header.Read(buf)
	}
	return header
}
func (p *VoiceHeader) Read(buf *bytes.Buffer) {
	binary.Read(buf, binary.LittleEndian, &p.Cmd)
	binary.Read(buf, binary.LittleEndian, &p.Length)
}
func (p *VoiceHeader) Write(buf *bytes.Buffer) {
	binary.Write(buf, binary.LittleEndian, p.Cmd)
	binary.Write(buf, binary.LittleEndian, p.Length)
}

func GetBytes(cb func(*bytes.Buffer)) []byte {
	buf := make([]byte, 0, MSG_CONST_MAX_PACKET_SIZE)
	bbuf := bytes.NewBuffer(buf)
	cb(bbuf)
	return bbuf.Bytes()
}

type VoiceTransfer struct {
	UDP        *net.UDPConn
}

func (this *VoiceTransfer) ReadPacket() (*net.UDPAddr, interface{}, *VoiceMessage, []byte, error) {
	buf := make([]byte, MSG_CONST_MAX_VOICE_PACKET_SIZE)
	l, addr, err := this.UDP.ReadFromUDP(buf)
	if err != nil {
		return nil, nil, nil, nil, err
	}
	bbuf := bytes.NewBuffer(buf[0:l])
	header := NewVoiceHeader(bbuf)
	msg := &VoiceMessage{}
	err = proto.Unmarshal(bbuf.Bytes(), msg)
	if err != nil {
		return nil, nil, nil, nil, err
	}
	return addr, header, msg, buf[0:l], nil
}
func (this *VoiceTransfer) WritePacket(cmd uint32, msg *VoiceMessage, peer *net.UDPAddr) error {
	header := NewVoiceHeader(nil)
	header.Cmd = uint8(cmd)
	body, _ := proto.Marshal(msg)
	header.Length = uint16(len(body))
	buf := bytes.NewBuffer(make([]byte, 0, MSG_CONST_MAX_VOICE_PACKET_SIZE))
	header.Write(buf)
	buf.Write(body)
	_, err := this.UDP.WriteToUDP(buf.Bytes(), peer)
	return err
}
func (this *VoiceTransfer) GetConnection() *net.UDPConn {
	return this.UDP
}