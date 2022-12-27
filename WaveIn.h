//---------------------------------------------------------------------------

#ifndef WaveInH
#define WaveInH

#include <windows.h>
#include <MMSystem.h>

#include <array>
#include <vector>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include <Winapi.Messages.hpp>
#include <System.SyncObjs.hpp>
#include <System.SysUtils.hpp>

#include <memory>

namespace App {

template<typename T>
class WaveIn {
public:
    using SampleType = T;
    using BufferCont = std::vector<SampleType>;

    WaveIn( int Device, size_t Channels, size_t SampleCount, uint32_t SamplesPerSec );
    virtual ~WaveIn();
    WaveIn( WaveIn const & ) = delete;
    WaveIn( WaveIn&& ) = delete;
    WaveIn& operator=( WaveIn const & ) = delete;
    WaveIn& operator=( WaveIn&& ) = delete;
    void Start();
    void Stop();
    [[nodiscard]] bool IsRunning() const { return !stopped_; }
protected:
    static constexpr size_t BufferCount = 2;

    WAVEFORMATEX waveFormat_;
    HWAVEIN waveHandle_;
    BufferCont waveData_[BufferCount];
    WAVEHDR waveHeader_[BufferCount];

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

template<typename T = int16_t>
class WaveInCO : public WaveIn<T> {
public:
    using CallableObjType = std::function<void(typename WaveIn<T>::BufferCont const &)>;

    WaveInCO( int Device, size_t Channels, size_t SampleCount, uint32_t SamplesPerSec,
              CallableObjType Callback )
      : WaveIn<T>( Device, Channels, SampleCount, SamplesPerSec )
      , callback_{ Callback }
    {
    }
protected:
    virtual void DoCallback( typename WaveIn<T>::BufferCont const & WaveData ) override {
        callback_( WaveData );
    }
private:
    CallableObjType callback_;
};

template<typename T>
WaveIn<T>::WaveIn( int Device, size_t Channels, size_t SampleCount, uint32_t SamplesPerSec )
{
    for ( auto& WD : waveData_ ) {
        WD.resize( SampleCount );
    }

    waveFormat_ = { 0 };
    waveFormat_.wFormatTag      = WAVE_FORMAT_PCM;
    waveFormat_.nChannels       = Channels;
    waveFormat_.nSamplesPerSec  = SamplesPerSec;
    waveFormat_.wBitsPerSample  = sizeof( SampleType ) * 8;
    waveFormat_.nAvgBytesPerSec =
        SamplesPerSec *
        waveFormat_.wBitsPerSample / 8 *
        waveFormat_.nChannels;
    waveFormat_.nBlockAlign     =
        waveFormat_.nChannels * waveFormat_.wBitsPerSample / 8;
    waveFormat_.cbSize          = 0;

    auto Res = ::waveInOpen(
        &waveHandle_,
        Device,
        &waveFormat_,
        reinterpret_cast<DWORD_PTR>( &WaveInProc ),
        reinterpret_cast<DWORD_PTR>( this ),
        CALLBACK_FUNCTION
    );
    if ( Res ) {
        RaiseLastOSError();
    }
}

template<typename T>
WaveIn<T>::~WaveIn()
{
    Stop();
    ::waveInClose( waveHandle_ );
}
//---------------------------------------------------------------------------

template<typename T>
void WaveIn<T>::Start()
{
    stopReq_ = false;
    stopped_ = false;

    exitEvt_->ResetEvent();

    for ( size_t Idx {} ; Idx < BufferCount ; ++Idx ) {
        auto& WaveHdr = waveHeader_[Idx];
        auto& WaveData = waveData_[Idx];
        WaveHdr = { 0 };
        WaveHdr.dwBufferLength = WaveData.size() * sizeof( SampleType );
        WaveHdr.dwFlags        = 0;
        WaveHdr.lpData         = reinterpret_cast<LPSTR>( WaveData.data() );
        WaveHdr.dwUser         = Idx;

        if ( ::waveInPrepareHeader( waveHandle_, &WaveHdr, sizeof WaveHdr ) ) {
            RaiseLastOSError();
        }
    }

    for ( auto& WaveHdr : waveHeader_ ) {
        if ( ::waveInAddBuffer( waveHandle_, &WaveHdr, sizeof WaveHdr ) ) {
            RaiseLastOSError();
        }
    }

    if ( ::waveInStart( waveHandle_ ) ) {
        RaiseLastOSError();
    }
}
//---------------------------------------------------------------------------

template<typename T>
void WaveIn<T>::Stop()
{
    if ( !stopped_ ) {
        stopReq_ = true;
        ::WaitForSingleObject( reinterpret_cast<HANDLE>( exitEvt_->Handle ), INFINITE );
        ::waveInReset( waveHandle_ );
        //::Sleep( 100 );
        ::waveInStop( waveHandle_ );
        for ( auto& WaveHdr : waveHeader_ ) {
            if ( ::waveInUnprepareHeader( waveHandle_, &WaveHdr, sizeof WaveHdr ) ) {
                RaiseLastOSError();
            }
        }
        ::waveInClose( waveHandle_ );
        stopped_ = true;
    }
}
//---------------------------------------------------------------------------

template<typename T>
void WaveIn<T>::WaveInProc( HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
                         DWORD_PTR dwParam1, DWORD_PTR dwParam2 )
{
    auto This = reinterpret_cast<WaveIn*>( dwInstance );

    switch ( uMsg ) {
        case WIM_OPEN:
            break;
        case WIM_DATA:
                if ( This->stopReq_ ) {
                    This->exitEvt_->SetEvent();
                }
                else {
                    if ( auto const Hdr = reinterpret_cast<WAVEHDR *>( dwParam1 ) ) {
                        if ( DWORD BytesRecorded = Hdr->dwBytesRecorded ) {
                            auto const BufferNo = Hdr->dwUser;
                            //Qui arriva l'audio
                            auto& WaveData = This->waveData_[BufferNo];
                            This->DoCallback( WaveData );
                            ::waveInAddBuffer( This->waveHandle_, Hdr, sizeof *Hdr );
                        }
                    }
                }
            break;
        case WIM_CLOSE:
            break;
    }
}

}
//---------------------------------------------------------------------------
#endif