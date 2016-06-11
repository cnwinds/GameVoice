package object

import "sync"

type Group struct {
	GroupID    uint64
	PlayerList map[uint64]*Player
	Count      int
	lock       sync.RWMutex
}

func NewGroup(groupID uint64) *Group {
	g := new(Group)
	g.GroupID = groupID
	g.PlayerList = make(map[uint64]*Player)
	return g
}

func (g *Group) ForeachPlayerList(cb func(k uint64, v *Player)) {
	g.lock.RLock()
	for k, v := range g.PlayerList {
		cb(k, v)
	}
	g.lock.RUnlock()
}

func (g *Group) FindPlayer(playerID uint64) *Player {
	g.lock.RLock()
	p, _ := g.PlayerList[playerID]
	g.lock.RUnlock()
	return p
}

func (g *Group) AddPlayer(p *Player) bool {
	g.lock.Lock()
	g.PlayerList[p.PlayerID] = p
	g.Count++
	g.lock.Unlock()
	return true
}

func (g *Group) DelPlayer(p *Player) bool {
	g.lock.Lock()
	delete(g.PlayerList, p.PlayerID)
	g.Count--
	g.lock.Unlock()
	return true
}

func (g *Group) DelPlayerByID(id uint64) bool {
	g.lock.Lock()
	delete(g.PlayerList, id)
	g.Count--
	g.lock.Unlock()
	return true
}
