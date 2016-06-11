// ConsoleApplication1.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "VoiceModule.h"
#include "VoiceProxy.h"

#define USE_OPUS_ENCODE     1

#define SAMPLES_PER_FRAME   ((SAMPLES_PER_SEC / 1000) * FRAME_CYCLE)

#define FRAME_PER_SEC       (1000 / FRAME_CYCLE)
#define SIZE_PER_SEC        (SAMPLES_PER_SEC * CHANNELS * BITS_PER_SAMPLE / 8)
#define SIZE_PER_FRAME      (SIZE_PER_SEC * FRAME_CYCLE / 1000)

#define BUFFER_BLOCK        5
#define BUFFER_SIZE         (SIZE_PER_FRAME * BUFFER_BLOCK)
#define NOTIFY_CNT          BUFFER_BLOCK


void VoiceModule::Initialize()
{
    LogInfo("SAMPLES_PER_FRAME=%d, SIZE_PER_FRAME=%d", SAMPLES_PER_FRAME, SIZE_PER_FRAME);

    m_IsCaptureRunning = false;
    m_IsPlayRunning = false;

    InitializeCriticalSection(&m_JBufferLock);
    InitializeCriticalSection(&m_PlayBufferLock);

    // 初始化speexdsp
    m_pJBuffer = jitter_buffer_init(FRAME_CYCLE);
    jitter_buffer_reset(m_pJBuffer);
    int sampleRate = SAMPLES_PER_SEC;
    m_pSpeexAec = speex_echo_state_init(SAMPLES_PER_FRAME, SAMPLES_PER_FRAME * 20);
    speex_echo_ctl(m_pSpeexAec, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);
    m_pSpeexPreprocess = speex_preprocess_state_init(SAMPLES_PER_FRAME, SAMPLES_PER_SEC);
    speex_preprocess_ctl(m_pSpeexPreprocess, SPEEX_PREPROCESS_SET_ECHO_STATE, m_pSpeexAec);

    int denoise = 1;
    speex_preprocess_ctl(m_pSpeexPreprocess, SPEEX_PREPROCESS_SET_DENOISE, &denoise);
    int agc = 0, agcLevel = 8000;
    speex_preprocess_ctl(m_pSpeexPreprocess, SPEEX_PREPROCESS_SET_AGC, &agc);
    speex_preprocess_ctl(m_pSpeexPreprocess, SPEEX_PREPROCESS_SET_AGC_LEVEL, &agcLevel);
    int dereverb = 0;
    speex_preprocess_ctl(m_pSpeexPreprocess, SPEEX_PREPROCESS_SET_DEREVERB, &dereverb);
    float decay = .0;
    speex_preprocess_ctl(m_pSpeexPreprocess, SPEEX_PREPROCESS_SET_DEREVERB_DECAY, &decay);
    float dereverb_level = .0;
    speex_preprocess_ctl(m_pSpeexPreprocess, SPEEX_PREPROCESS_SET_DEREVERB_LEVEL, &dereverb_level);

    m_notifyEvent[MIC_EVENT] = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_notifyEvent[PLAY_EVENT] = CreateEvent(NULL, FALSE, FALSE, NULL);

    m_pPlayBuffer = util_cbuf_create(Max2nUp(BUFFER_SIZE));

    // 初始化opus编码器
    int err;
    m_pOpusEncoder = opus_encoder_create(SAMPLES_PER_SEC, CHANNELS, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK || m_pOpusEncoder == NULL) return;
    //opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(0));
    opus_encoder_ctl(m_pOpusEncoder, OPUS_SET_COMPLEXITY(10));
    // opus_encoder_ctl(enc, OPUS_SET_VBR(1));
    // opus_encoder_ctl(enc, OPUS_SET_VBR_CONSTRAINT(1));
    opus_encoder_ctl(m_pOpusEncoder, OPUS_SET_DTX(1));
    //opus_encoder_ctl(enc, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_NARROWBAND));
    opus_encoder_ctl(m_pOpusEncoder, OPUS_SET_INBAND_FEC(1));

    m_OpusDecoder = opus_decoder_create(SAMPLES_PER_SEC, CHANNELS, &err);
    if (err != OPUS_OK || m_OpusDecoder == NULL) return;

    m_pParent = NULL;
}

void VoiceModule::Finalize()
{
    jitter_buffer_destroy(m_pJBuffer);
    speex_echo_state_destroy(m_pSpeexAec);
    m_pJBuffer = NULL;

    DeleteCriticalSection(&m_JBufferLock);
    DeleteCriticalSection(&m_PlayBufferLock);

    CloseHandle(m_notifyEvent[MIC_EVENT]);
    CloseHandle(m_notifyEvent[PLAY_EVENT]);

    util_cbuf_release(m_pPlayBuffer);
}

void VoiceModule::JitterPut(PVOID pData, int Len, int TimeStamp)
{
    JitterBufferPacket p;
    p.data = (char *)pData;
    p.len = Len;
    p.timestamp = TimeStamp;
    p.span = FRAME_CYCLE;
    EnterCriticalSection(&m_JBufferLock);
    jitter_buffer_put(m_pJBuffer, &p);
    LeaveCriticalSection(&m_JBufferLock);
}

bool VoiceModule::JitterGet(PVOID pBuffer, int &Len)
{
    JitterBufferPacket packet;
    packet.data = (char *)pBuffer;
    packet.len = Len;
    EnterCriticalSection(&m_JBufferLock);
    int ret = jitter_buffer_get(m_pJBuffer, &packet, FRAME_CYCLE, NULL);
    jitter_buffer_tick(m_pJBuffer);
    LeaveCriticalSection(&m_JBufferLock);
    if (ret == JITTER_BUFFER_OK) {
        Len = packet.len;
    }
    return ret == JITTER_BUFFER_OK;
}

DWORD WINAPI VoiceModule::ThreadCaputerMIC(_In_ LPVOID lpParameter)
{
    VoiceModule *This = (VoiceModule *)lpParameter;
    HRESULT hr;
    int nLastCaputerOffset = 0;
    int nCaputerFrames = 0;

    while (This->IsCaptureRunning())
    {
        DWORD rc = WaitForSingleObject(This->m_notifyEvent[MIC_EVENT], INFINITE);
        if (rc != WAIT_OBJECT_0)
        {
            LogError("WaitForSingleObject failed, rc=%d, errorcode=%d", rc, GetLastError());
            break;
        }

        if (!This->IsCaptureRunning()) break;

        // 延迟一帧开始捕捉数据
        if (++nCaputerFrames > 1)
        {
            // 获取数据
            LONG lLockSize;
            DWORD dwCapturePos, dwReadPos;
            if (FAILED(hr = This->m_SoundCaputerBuffer->GetCurrentPosition(&dwCapturePos, &dwReadPos)))
                break;

            lLockSize = dwReadPos - nLastCaputerOffset;
            if (lLockSize < 0) lLockSize += BUFFER_SIZE;
            lLockSize -= (lLockSize % SIZE_PER_FRAME);
            if (lLockSize == 0) continue;

            lLockSize = SIZE_PER_FRAME;

            VOID*   pbCaptureData = NULL;
            DWORD   dwCaptureLength;
            VOID*   pbCaptureData2 = NULL;
            DWORD   dwCaptureLength2;
            if (FAILED(hr = This->m_SoundCaputerBuffer->Lock(nLastCaputerOffset, lLockSize,
                &pbCaptureData, &dwCaptureLength, &pbCaptureData2, &dwCaptureLength2, 0L)))
            {
                LogError("DXAudioCaputer: Lock failed: hr=0x%08lx", hr);
                break;
            }

            unsigned char MicData[SIZE_PER_FRAME];
            if (pbCaptureData && dwCaptureLength)
                CopyMemory(MicData, pbCaptureData, dwCaptureLength);
            if (pbCaptureData2 && dwCaptureLength2)
                CopyMemory(&MicData[dwCaptureLength], pbCaptureData2, dwCaptureLength2);
            This->m_SoundCaputerBuffer->Unlock(pbCaptureData, dwCaptureLength, pbCaptureData2, dwCaptureLength2);

            nLastCaputerOffset = (nLastCaputerOffset + lLockSize) % BUFFER_SIZE;

            // 消除回声
            short PlayData[SAMPLES_PER_FRAME], CleanData[SAMPLES_PER_FRAME];
            if (util_cbuf_data(This->m_pPlayBuffer) >= SIZE_PER_FRAME) {
                EnterCriticalSection(&This->m_PlayBufferLock);
                util_cbuf_load(This->m_pPlayBuffer, PlayData, SIZE_PER_FRAME);
                LeaveCriticalSection(&This->m_PlayBufferLock);
            }
            else {
                ZeroMemory(PlayData, sizeof(PlayData));
            }
            speex_echo_cancellation(This->m_pSpeexAec, (short*)MicData, PlayData, CleanData);

            // 降噪处理
            speex_preprocess_run(This->m_pSpeexPreprocess, (short*)CleanData);
            // 编码处理
            unsigned char buffer[VOICE_BUFFER];
            int len;
            if (USE_OPUS_ENCODE) {
                len = opus_encode(This->m_pOpusEncoder, (short*)CleanData, SAMPLES_PER_FRAME, buffer, sizeof(buffer));
                if ((len > 1) && (This->m_pParent)) {
                    // 发送数据
                    This->m_pParent->SendVoiceData(nCaputerFrames, (char *)buffer, len);
                }
            }
            else {
                len = sizeof(MicData);
                if (This->m_pParent) {
                    This->m_pParent->SendVoiceData(nCaputerFrames, (char *)MicData, len);
                }
            }
            
            // 统计日志
            static int LogSendSize = 0;
            LogSendSize += len > 1 ? len : 0;
            if (nCaputerFrames % FRAME_PER_SEC == 0) {
                static int lastValue = 0;
                LogInfo("send size=%d, size per second=%d", LogSendSize, LogSendSize - lastValue);
                lastValue = LogSendSize;

                //int count = 0;
                //jitter_buffer_ctl(m_jbuffer, JITTER_BUFFER_GET_AVALIABLE_COUNT, &count);
                //LogInfo("jitter buffer count=%d", count);
            }
        }
    }

    if (This->m_SoundCaputerBuffer)
    {
        This->m_SoundCaputerBuffer->Stop();
        This->m_SoundCaputerBuffer->Release();
    }
    if (This->m_SoundCaputer)
        This->m_SoundCaputer->Release();

    This->m_SoundCaputerBuffer = NULL;
    This->m_SoundCaputer = NULL;
    return 1;
}

DWORD WINAPI VoiceModule::ThreadPlayStream(_In_ LPVOID lpParameter)
{
    VoiceModule *This = (VoiceModule *)lpParameter;

    HRESULT hr;
    int nLastPlayOffset = 0;
    int nPlayFrames = 0;
    bool bInit = false;

    while (This->IsPlayRunning())
    {
        DWORD rc = WaitForSingleObject(This->m_notifyEvent[PLAY_EVENT], INFINITE);
        if (rc != WAIT_OBJECT_0) {
            LogError("WaitForSingleObject failed, rc=%d, errorcode=%d", rc, GetLastError());
            break;
        }

        if (!This->IsPlayRunning()) break;

        // 解码数据
        unsigned char PacketData[VOICE_BUFFER];
        short PlayDataTemp[SAMPLES_PER_FRAME];
        int TmpLen = sizeof(PacketData);
        if (This->JitterGet(PacketData, TmpLen)) {
            if (USE_OPUS_ENCODE) {
                opus_decode(This->m_OpusDecoder, PacketData, TmpLen, PlayDataTemp, SAMPLES_PER_FRAME, 0);
            }
            else {
                CopyMemory(PlayDataTemp, PacketData, TmpLen);
            }
        }
        else {
            if (USE_OPUS_ENCODE) {
                opus_decode(This->m_OpusDecoder, NULL, 0, PlayDataTemp, SAMPLES_PER_FRAME, 1);
            }
            else {
                ZeroMemory(PlayDataTemp, TmpLen);
            }
        }

        // 首次计算写入位置
        if (!bInit) {
            bInit = true;
            DWORD dwPlayPosition, dwWritePosition;
            if (FAILED(hr = This->m_SoundPlayBuffer->GetCurrentPosition(&dwPlayPosition, &dwWritePosition))) {
                LogError("DXAudioOutput: GetCurrentPosition failed: hr=0x%08lx", hr);
                break;
            }
            nLastPlayOffset = (dwWritePosition + 1 * SIZE_PER_FRAME);
            nLastPlayOffset = nLastPlayOffset - nLastPlayOffset % SIZE_PER_FRAME;
        }
        // 写入数据
        VOID *pPlayData, *pPlayData2;
        DWORD nPlayLen, nPlayLen2;
        if (FAILED(hr = This->m_SoundPlayBuffer->Lock(nLastPlayOffset, SIZE_PER_FRAME, &pPlayData, &nPlayLen, &pPlayData2, &nPlayLen2, 0))) {
            LogError("DXAudioOutput: Lock failed: hr=0x%08lx", hr);
            break;
        }
        if (pPlayData && nPlayLen)
            CopyMemory(pPlayData, PlayDataTemp, nPlayLen);
        if (pPlayData2 && nPlayLen2)
            CopyMemory(pPlayData2, &PlayDataTemp[nPlayLen / 2], nPlayLen2);
        This->m_SoundPlayBuffer->Unlock(pPlayData, nPlayLen, pPlayData2, nPlayLen2);

        nLastPlayOffset = (nLastPlayOffset + SIZE_PER_FRAME) % BUFFER_SIZE;
        nPlayFrames++;

        EnterCriticalSection(&This->m_PlayBufferLock);
        util_cbuf_save(This->m_pPlayBuffer, PlayDataTemp, SIZE_PER_FRAME);
        LeaveCriticalSection(&This->m_PlayBufferLock);
    }
    if (This->m_SoundPlayBuffer) {
        This->m_SoundPlayBuffer->Stop();
        This->m_SoundPlayBuffer->Release();
    }
    if (This->m_SoundPlay)
        This->m_SoundPlay->Release();

    This->m_SoundPlayBuffer = NULL;
    This->m_SoundPlay = NULL;
    return 1;
}

bool VoiceModule::StartCaputerMIC(WAVEFORMATEX & wfxIn, VoiceServerProxy *pParent)
{
    m_pParent = pParent;

    const GUID * pguidCapture = &(GUID_NULL);

    HRESULT hr;
    if (FAILED(hr = DirectSoundCaptureCreate8(pguidCapture, &m_SoundCaputer, 0)))
    {
        LogError("%s: DirectSoundCaptureCreate8 err", __func__);
        return false;
    }

    DSCBUFFERDESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = 0;
    desc.dwBufferBytes = BUFFER_SIZE;
    desc.lpwfxFormat = &wfxIn;
    LPDIRECTSOUNDCAPTUREBUFFER buffer;
    if (FAILED(hr = m_SoundCaputer->CreateCaptureBuffer(&desc, &buffer, 0)))
    {
        LogError("%s: CreateCaptureBuffer err\n", __func__);
        return false;
    }
    buffer->QueryInterface(IID_IDirectSoundCaptureBuffer8, (LPVOID*)&m_SoundCaputerBuffer);
    buffer->Release();

    LPDIRECTSOUNDNOTIFY8 notify;
    m_SoundCaputerBuffer->QueryInterface(IID_IDirectSoundNotify8, (LPVOID*)&notify);
    DSBPOSITIONNOTIFY dsPosNotify[NOTIFY_CNT];
    for (long index = 0; index < NOTIFY_CNT; index++)
    {
        dsPosNotify[index].dwOffset = index * SIZE_PER_FRAME + SIZE_PER_FRAME - 1;
        dsPosNotify[index].hEventNotify = m_notifyEvent[MIC_EVENT];
    }
    notify->SetNotificationPositions(NOTIFY_CNT, dsPosNotify);
    notify->Release();

    m_SoundCaputerBuffer->Start(DSCBSTART_LOOPING);

    m_IsCaptureRunning = true;
    m_ThreadCaputerMIC = CreateThread(NULL, 0, ThreadCaputerMIC, this, 0, NULL);
    return m_ThreadCaputerMIC != NULL;
}

bool VoiceModule::StartPlayStream(WAVEFORMATEX & wfxOut)
{
    const GUID * pguidPlay = &(GUID_NULL);

    HRESULT hr;
    if (FAILED(hr = DirectSoundCreate8(pguidPlay, &m_SoundPlay, NULL)))
    {
        LogError("Unable to create DirectSound object, hr=%x", hr);
        return false;
    }

    HWND hWnd = GetForegroundWindow();
    if (hWnd == NULL) hWnd = GetDesktopWindow();
    m_SoundPlay->SetCooperativeLevel(hWnd, DSSCL_NORMAL);

    DSBUFFERDESC dsbdesc;
    ZeroMemory(&dsbdesc, sizeof(dsbdesc));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_LOCSOFTWARE;
    dsbdesc.dwBufferBytes = BUFFER_SIZE;
    dsbdesc.lpwfxFormat = &wfxOut;

    LPDIRECTSOUNDBUFFER buffer;
    if (FAILED(hr = m_SoundPlay->CreateSoundBuffer(&dsbdesc, &buffer, NULL))) {
        LogError("DXAudioOutput: CreateSoundBuffer (Primary) failed: hr=0x%08lx", hr);
        return false;
    }
    if (FAILED(buffer->QueryInterface(IID_IDirectSoundBuffer8, (LPVOID*)&m_SoundPlayBuffer)))
    {
        LogError("DXAudioOutput: CreateSoundBuffer (Second) failed: hr=0x%08lx", hr);
        return false;
    }
    buffer->Release();

    LPDIRECTSOUNDNOTIFY8 notify;
    if (FAILED(m_SoundPlayBuffer->QueryInterface(IID_IDirectSoundNotify8, (LPVOID*)&notify)))
    {
        LogError("DXAudioOutput: Get Notify Interface failed: hr=0x%08lx", hr);
        return false;
    }
    DSBPOSITIONNOTIFY dsPosNotify[NOTIFY_CNT];
    for (long index = 0; index < NOTIFY_CNT; index++)
    {
        dsPosNotify[index].dwOffset = index * SIZE_PER_FRAME;
        dsPosNotify[index].hEventNotify = m_notifyEvent[PLAY_EVENT];
    }
    notify->SetNotificationPositions(NOTIFY_CNT, dsPosNotify);
    notify->Release();

    m_SoundPlayBuffer->SetCurrentPosition(0);
    m_SoundPlayBuffer->SetVolume(DSBVOLUME_MAX);
    m_SoundPlayBuffer->Play(0, 0, DSBPLAY_LOOPING);

    m_IsPlayRunning = true;
    m_ThreadPlayStream = CreateThread(NULL, 0, ThreadPlayStream, this, 0, NULL);
    return m_ThreadPlayStream != NULL;
}

void VoiceModule::StopCaptureMIC()
{
    m_IsCaptureRunning = false;
    WaitForSingleObject(m_ThreadCaputerMIC, INFINITE);
}

void VoiceModule::StopPlayStream()
{
    m_IsPlayRunning = false;
    WaitForSingleObject(m_ThreadPlayStream, INFINITE);

}
