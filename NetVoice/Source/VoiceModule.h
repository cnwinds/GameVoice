#pragma once
#include <stdint.h>
#include <windows.h>
#include <dsound.h>
#include <winsock.h>

#include "speex/speex_jitter.h"
#include "speex/speex_echo.h"
#include "speex/speex_preprocess.h"
#include "opus.h"

#include "Utils.h"
#include "CirBuffer.h"

class VoiceServerProxy;

class VoiceModule {

public:
    void    Initialize();
    void    Finalize();

public:
    bool    StartCaputerMIC(WAVEFORMATEX & wfxIn, VoiceServerProxy *pParent);
    void    StopCaptureMIC();
    bool    IsCaptureRunning() { return m_IsCaptureRunning; }

    bool    StartPlayStream(WAVEFORMATEX & wfxOut);
    void    StopPlayStream();
    bool    IsPlayRunning() { return m_IsPlayRunning; }

public:
    void    JitterPut(PVOID pData, int Len, int TimeStamp);
    bool    JitterGet(PVOID pBuffer, int &Len);
private:
    JitterBuffer        *m_pJBuffer;
    CRITICAL_SECTION    m_JBufferLock;

public:
    static DWORD WINAPI ThreadCaputerMIC(_In_ LPVOID lpParameter);
    static DWORD WINAPI ThreadPlayStream(_In_ LPVOID lpParameter);

private:
    tea_cirbuf_t        *m_pPlayBuffer;
    CRITICAL_SECTION    m_PlayBufferLock;

    enum { MIC_EVENT, PLAY_EVENT, EVENT_NUM };
    HANDLE m_notifyEvent[EVENT_NUM];

    LPDIRECTSOUNDCAPTURE8       m_SoundCaputer;
    LPDIRECTSOUNDCAPTUREBUFFER8 m_SoundCaputerBuffer;
    bool                        m_IsCaptureRunning;

    LPDIRECTSOUND8              m_SoundPlay;
    LPDIRECTSOUNDBUFFER8        m_SoundPlayBuffer;
    bool                        m_IsPlayRunning;

    SpeexEchoState              *m_pSpeexAec;
    SpeexPreprocessState        *m_pSpeexPreprocess;

    OpusEncoder                 *m_pOpusEncoder;
    OpusDecoder                 *m_OpusDecoder;

    uint32_t sessionID;
    VoiceServerProxy            *m_pParent;
    HANDLE                      m_ThreadCaputerMIC;
    HANDLE                      m_ThreadPlayStream;
};
