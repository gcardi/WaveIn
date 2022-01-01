//---------------------------------------------------------------------------

#ifndef WaveInH
#define WaveInH

#include <windows.h>
#include <MMSystem.h>

#include <array>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include <Winapi.Messages.hpp>
#include <System.SyncObjs.hpp>

#include <memory>

class WaveIn {
public:
    static constexpr int SampleCount = 512;
    static constexpr uint32_t SamplesPerSec = 22050;
    using BufferCont = std::array<int16_t,SampleCount>;

    WaveIn( int Device );
    virtual ~WaveIn();
    WaveIn( WaveIn const & ) = delete;
    WaveIn( WaveIn&& ) = delete;
    WaveIn& operator=( WaveIn const & ) = delete;
    WaveIn& operator=( WaveIn&& ) = delete;
    void Start();
    void Stop();
protected:
    WAVEFORMATEX waveFormat_;
    HWAVEIN waveHandle_;
    BufferCont waveData_[2];
    WAVEHDR waveHeader_[2];

    std::atomic<bool> stopReq_ { false };
    std::atomic<bool> stopped_ { false };

    virtual void DoCallback( BufferCont const & WaveData ) = 0;

    static void CALLBACK WaveInProc( HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
                                     DWORD_PTR dwParam1, DWORD_PTR dwParam2 );
private:

    std::unique_ptr<TEvent> exitEvt_ {
        new TEvent( 0, false, false, EmptyStr, false )
    };
};

class WaveInCO : public WaveIn {
public:
    using CallableObjType = std::function<void(BufferCont const &)>;

    WaveInCO( int Device, CallableObjType Callback )
      : WaveIn( Device )
      , callback_{ Callback }
    {
    }
protected:
    virtual void DoCallback( BufferCont const & WaveData ) override {
        callback_( WaveData );
    }
private:
    CallableObjType callback_;
};

//---------------------------------------------------------------------------
#endif
