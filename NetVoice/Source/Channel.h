#pragma once
#include <stdint.h>
#include <Windows.h>
#include <list>

using namespace std;

template<typename t>
class Channel {
public:
    Channel(int Cap = 1);
    ~Channel();

    void Put(const t &Value);
    bool Get(t &Value);
    int Count();

private:
    list<t>             m_List;
    unsigned int        m_Count;
    unsigned int        m_Cap;
    CRITICAL_SECTION    m_Lock;
    HANDLE              m_NotifyGet;
    HANDLE              m_NotifyPut;
};

template<typename t>
Channel<typename t>::Channel(int Cap = 1) {
    m_Count = 0;
    m_Cap = Cap;
    InitializeCriticalSection(&m_Lock);
    m_NotifyGet = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_NotifyPut = CreateEvent(NULL, FALSE, FALSE, NULL);
}

template<typename t>
Channel<typename t>::~Channel() {
    DeleteCriticalSection(&m_Lock);
    CloseHandle(m_NotifyGet);
    CloseHandle(m_NotifyPut);
}

template<typename t>
void Channel<typename t>::Put(const t &Value) {
    EnterCriticalSection(&m_Lock);
    if (m_Count >= m_Cap) {
        LeaveCriticalSection(&m_Lock);
        DWORD rc = WaitForSingleObject(m_NotifyPut, INFINITE);
        return Put(Value);
    }
    else {
        m_List.push_back(Value);
        LeaveCriticalSection(&m_Lock);
        InterlockedIncrement(&m_Count);
        SetEvent(m_NotifyGet);
    }
}

template<typename t>
bool Channel<typename t>::Get(t &Value) {
    EnterCriticalSection(&m_Lock);
    if (m_Count > 0) {
        Value = m_List.front();
        m_List.pop_front();
        LeaveCriticalSection(&m_Lock);
        InterlockedDecrement(&m_Count);
        SetEvent(m_NotifyPut);
        return true;
    }
    else {
        LeaveCriticalSection(&m_Lock);
        DWORD rc = WaitForSingleObject(m_NotifyGet, INFINITE);
        if (rc == WAIT_OBJECT_0) {
            return Get(Value);
        }
        else {
            return false;
        }
    }
}

template<typename t>
int Channel<typename t>::Count() {
    return m_Count;
}