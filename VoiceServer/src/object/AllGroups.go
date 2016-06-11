package object

import (
	"sync"
	"utils"
)

const MAX_PLAYER int = 65535

type Groups struct {
	SessionKey     uint64
	GroupList      map[uint64]*Group
	groupListLock  sync.RWMutex
	PlayerList     map[uint64]*Player
	playerListLock sync.RWMutex
	PlayerIndex    *FastIndex
}

func NewAllGroups() *Groups {
	ag := new(Groups)
	ag.GroupList = make(map[uint64]*Group)
	ag.PlayerList = make(map[uint64]*Player)
	ag.PlayerIndex = NewFastIndex(MAX_PLAYER)
	ag.SessionKey = 1234
	return ag
}

func (ag *Groups) CreatePlayer(playerID uint64, groupID uint64) *Player {
	if ag.PlayerIndex.Count >= MAX_PLAYER {
		utils.ErrInfo("create player too much. current player count=%d", ag.PlayerIndex.Count)
		return nil
	}

	p := ag.FindPlayer(playerID)
	if p == nil {
		p = NewPlayer(playerID, groupID)
		ag.playerListLock.Lock()
		ag.PlayerList[playerID] = p
		ag.playerListLock.Unlock()
		p.PlayerIndex = uint32(ag.PlayerIndex.Add(p))
		g := ag.CreateGroup(groupID)
		g.AddPlayer(p)
	}

	return p
}

func (ag *Groups) FindPlayer(playerID uint64) *Player {
	ag.playerListLock.RLock()
	v, _ := ag.PlayerList[playerID]
	ag.playerListLock.RUnlock()
	return v
}

func (ag *Groups) FindPlayerByIndex(index uint32, playerID uint64) *Player {
	v := ag.PlayerIndex.Get(int(index))
	if v != nil {
		p, _ := v.(*Player)
		if p.PlayerID == playerID {
			return p
		}
	}
	return nil
}

func (ag *Groups) DelPlayer(playerID uint64) bool {
	ag.playerListLock.RLock()
	v, exist := ag.PlayerList[playerID]
	ag.playerListLock.RUnlock()

	if exist {
		ag.playerListLock.Lock()
		delete(ag.PlayerList, playerID)
		ag.playerListLock.Unlock()
		ag.PlayerIndex.DelByIndex(int(v.PlayerIndex))

		ag.groupListLock.RLock()
		g, exist := ag.GroupList[v.GroupID]
		ag.groupListLock.RUnlock()
		if exist {
			g.DelPlayerByID(playerID)
		}
	}
	if len(ag.GroupList) == 0 {
		ag.DelGroup(v.GroupID)
	}
	return true
}

func (ag *Groups) CreateGroup(groupID uint64) *Group {
	g := ag.FindGroup(groupID)
	if g == nil {
		g = NewGroup(groupID)
		ag.groupListLock.Lock()
		ag.GroupList[groupID] = g
		ag.groupListLock.Unlock()
	}
	return g
}

func (ag *Groups) FindGroup(groupID uint64) *Group {
	ag.groupListLock.RLock()
	v, _ := ag.GroupList[groupID]
	ag.groupListLock.RUnlock()
	return v
}

func (ag *Groups) DelGroup(groupID uint64) bool {
	g := ag.FindGroup(groupID)
	if g != nil {
		ag.playerListLock.Lock()
		for k, v := range g.PlayerList {
			delete(ag.PlayerList, k)
			ag.PlayerIndex.DelByIndex(int(v.PlayerIndex))
		}
		ag.playerListLock.Unlock()
		ag.groupListLock.Lock()
		delete(ag.GroupList, groupID)
		ag.groupListLock.Unlock()
	}
	return true
}
