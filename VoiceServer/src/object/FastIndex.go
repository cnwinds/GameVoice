package object

import "sync"

type FastIndex struct {
	list       []interface{}
	Count      int
	maxCount   int
	nextAddPos int
	lock       sync.RWMutex
}

func NewFastIndex(cap int) *FastIndex {
	r := new(FastIndex)
	r.list = make([]interface{}, cap)
	r.maxCount = cap
	return r
}

func (this *FastIndex) Add(v interface{}) int {
	c := 0
	total := this.maxCount
	this.lock.Lock()
	for p := this.nextAddPos; c < total; p = (p + 1) % total {
		if this.list[p] == nil {
			this.list[p] = v
			this.lock.Unlock()
			this.Count++
			this.nextAddPos = p + 1
			return p
		}
		c++
	}
	this.lock.Unlock()
	return -1
}
func (this *FastIndex) Get(pos int) interface{} {
	if pos < this.maxCount {
		this.lock.RLock()
		r := this.list[pos]
		this.lock.RUnlock()
		return r
	}
	return nil
}
func (this *FastIndex) DelByIndex(pos int) {
	if pos < this.maxCount {
		this.lock.Lock()
		this.list[pos] = nil
		this.lock.Unlock()
	}
}
