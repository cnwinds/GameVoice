package handle

import (
	"message"
	"net"
	"time"
	"utils"
)

type GameService struct {
	listen     *net.TCPListener
	handlerMap []message.HandleFunction
}

func NewGameService() *GameService {
	this := GameService{}
	this.handlerMap = make([]message.HandleFunction, 1024)
	return &this
}

func (this *GameService) GameServiceStart(addr string) {
	tcpAddr, err := net.ResolveTCPAddr("tcp", addr)
	utils.CheckError(err, "")
	this.listen, err = net.ListenTCP("tcp", tcpAddr)
	utils.CheckError(err, "")
	utils.LogInfo("listen tcp %s", addr)
	// defer tcpListen.Close()

	go this.handleGameListen()
}

func (this *GameService) handleGameListen() {
	for {
		tcpConn, err := this.listen.AcceptTCP()
		if err == nil {
			go this.handleGameConn(tcpConn)
		}
	}
}

func (this *GameService) handleGameConn(conn *net.TCPConn) {
	gc := &message.GameTransfer{Conn: conn}
	defer conn.Close()

	// 建立连接后30秒内要收到第一个包
	gc.SetReadTimeout(time.Now().Add(time.Second * 30))
	header, msg, err := gc.ReadPacket()
	if err != nil {
		utils.LogInfo("first packet error. err=%s", err.Error())
		return
	}
	// 后续包无时间限制
	gc.SetReadTimeout(time.Time{})
	for {
		h, _ := header.(*message.GameHeader)
		f := this.handlerMap[h.GetCmd()]
		if f != nil {
			if err == nil {
				f(gc, msg)
			} else {
				utils.ErrInfo("unmarshal packet error %s", err.Error())
				return
			}
		} else {
			utils.ErrInfo("unknow command id=%d", h.GetCmd())
		}

		header, msg, err = gc.ReadPacket()
		if err != nil {
			return
		}
	}
}

func (this *GameService) RegisterCmdHandler(cmd message.MSG, f message.HandleFunction) {
	this.handlerMap[uint32(cmd)] = f
}
