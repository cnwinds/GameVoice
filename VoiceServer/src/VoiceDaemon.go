package main

import (
	"handle"
	"message"
	"os"
	"os/signal"
	//"runtime/pprof"
	"config"
)

func main() {
	handle.Init()
	config.Init()

	gs := handle.NewGameService()
	gs.RegisterCmdHandler(message.MSG_HANDSHAKE, handle.HandleHandshake)
	gs.RegisterCmdHandler(message.MSG_ENTER_GROUP, handle.HandleEnterGroup)
	gs.RegisterCmdHandler(message.MSG_LEAVE_GROUP, handle.HandleLeaveGroup)
	gs.RegisterCmdHandler(message.MSG_ADJUST_PLAYER_ATTR, handle.HandleAdjustPlayerAttr)
	gs.RegisterCmdHandler(message.MSG_ADJUST_GROUP_ATTR, handle.HandleAdjustGroupAttr)
	gs.GameServiceStart("0.0.0.0:9000")

	vs := handle.NewVoiceService()
	vs.RegisterCmdHandler(message.VOICE_MSG_REQEUST_CONNECT, handle.HandleRequestConnect)
	vs.RegisterCmdHandler(message.VOICE_MSG_VOICE_DATA, handle.HandleVoiceData)
	vs.RegisterCmdHandler(message.VOICE_MSG_VOICE_HEARTBEAT, handle.HandleVoiceHeartbeat)
	vs.VoiceServiceStart("0.0.0.0:9000")

	// 性能监控
	//f, _ := os.Create("profile_file")
	//pprof.StartCPUProfile(f)
	//defer pprof.StopCPUProfile()

	// handle.Wait()

	// 等待信号退出
	waitSignal := make(chan os.Signal, 1)
	signal.Notify(waitSignal, os.Interrupt, os.Kill)
	<-waitSignal
}
