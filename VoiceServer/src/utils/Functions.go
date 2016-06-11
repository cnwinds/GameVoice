package utils

import (
	"fmt"
	"time"
)

func CheckError(err error, desc string) {
	if err != nil {
		panic("ERROR:" + err.Error() + " " + desc)
	}
}

func LogInfo(info string, args ...interface{}) {
	log("INFO", info, args...)
}

func ErrInfo(info string, args ...interface{}) {
	log("ERROR", info, args...)
}

func log(t string, info string, args ...interface{}) {
	fmt.Printf("[%s]", t)
	n := time.Now()
	fmt.Print(n.Format("(2006-01-02 15:04:05)-"))
	fmt.Printf(info, args...)
	fmt.Println("")
}
