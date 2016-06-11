package handle

import (
	"message"
	"net"
	"time"
	"utils"
)

type VoiceService struct {
	udpConn    *net.UDPConn
	handlerMap []message.HandleUDPFunction
}

func NewVoiceService() *VoiceService {
	this := VoiceService{}
	this.handlerMap = make([]message.HandleUDPFunction, 1024)
	return &this
}
func (this *VoiceService) VoiceServiceStart(udpAddrStr string) {
	utils.LogInfo("listen udp %s", udpAddrStr)
	udpAddr, err := net.ResolveUDPAddr("udp", udpAddrStr)
	utils.CheckError(err, "")
	this.udpConn, err = net.ListenUDP("udp", udpAddr)
	utils.CheckError(err, "")
	// defer udpConn.Close()

	go this.handleUdpPacket()
}

func (this *VoiceService) handleUdpPacket() {

	go func() {
		// log
		for {
			time.Sleep(time.Second * 5)
			for _, v := range AllGroups.GroupList {
				utils.LogInfo("groupid=%d, playercount=%d", v.GroupID, v.Count)
			}
		}
	}()

	for {
		vt := &message.VoiceTransfer{UDP: this.udpConn}
		peer, header, msg, packet, err := vt.ReadPacket()
		if err != nil {
			utils.LogInfo("read from udp error %s", err.Error())
			continue
		}
		h, _ := header.(*message.VoiceHeader)
		f := this.handlerMap[h.Cmd]
		if f != nil {
			go f(vt, peer, msg, packet)
		} else {
			utils.LogInfo("unknow command id=%d", h.Cmd)
		}
	}
}

func (this *VoiceService) RegisterCmdHandler(cmd message.VOICE_MSG, f message.HandleUDPFunction) {
	this.handlerMap[cmd] = f
}
