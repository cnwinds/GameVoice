package handle

import (
	"object"
	"time"
)

var (
	AllGroups *object.Groups
)

func Init() {
	AllGroups = object.NewAllGroups()
}

func Wait() {
	for {
		time.Sleep(time.Second * 60)
	}
}
