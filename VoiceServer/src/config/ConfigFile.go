package config

import (
	"encoding/json"
	"os"
)

var (
	configAll configItem
)

type verifyItem struct {
	IP string
	SN string
}
type configItem struct {
	Verify []verifyItem `json:verify`
}

func Init() {
	loadFile()
}
func loadFile() {
	f, e := os.Open("config.json")
	if e != nil {
		return
	}
	defer f.Close()

	buf := make([]byte, 10*1024)
	l, e := f.Read(buf)

	json.Unmarshal(buf[:l], &configAll)

	//utils.LogInfo("%v", cs)
}
func Verify(snKey string, ip string) bool {
	for _, v := range configAll.Verify {
		if v.IP == ip && v.SN == snKey {
			return true
		}
	}
	return false
}
