#pragma once

#include <xaudio2.h>

class FVoiceCallback : public IXAudio2VoiceCallback
{
public:
	void STDMETHODCALLTYPE OnBufferEnd(void* Context) override;

	void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32 BytesRequired) override {}
	void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
	void STDMETHODCALLTYPE OnStreamEnd() override {}
	void STDMETHODCALLTYPE OnBufferStart(void* pBufferContext) override {}
	void STDMETHODCALLTYPE OnLoopEnd(void* pBufferContext) override {}
	void STDMETHODCALLTYPE OnVoiceError(void* pBufferContext, HRESULT Error) override {}
};
