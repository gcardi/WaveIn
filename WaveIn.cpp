//---------------------------------------------------------------------------

#pragma hdrstop

#include <System.SysUtils.hpp>

#include "WaveIn.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)

WaveIn::WaveIn( int Device )
{
    waveFormat_ = { 0 };
    waveFormat_.wFormatTag      = WAVE_FORMAT_PCM;
    waveFormat_.nChannels       = 1;
    waveFormat_.nSamplesPerSec  = SamplesPerSec;
    waveFormat_.wBitsPerSample  = 16;
    waveFormat_.nAvgBytesPerSec =
        waveFormat_.nSamplesPerSec *
        waveFormat_.wBitsPerSample / 8 *
        waveFormat_.nChannels;
    waveFormat_.nBlockAlign     = 2;
    waveFormat_.cbSize          = 0;

    /*
    auto Res = ::waveInOpen(
        &waveHandle_,
        Device,
        &waveFormat_,
        0,
        0,
        WAVE_FORMAT_QUERY
    );
    if ( Res ) {
        RaiseLastOSError();
    }
    */

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
//---------------------------------------------------------------------------

WaveIn::~WaveIn()
{
    if ( !stopped_ ) {
        Stop();
    }
    ::waveInClose( waveHandle_ );
}
//---------------------------------------------------------------------------

void WaveIn::Start()
{
    stopReq_ = false;
    stopped_ = false;

    exitEvt_->ResetEvent();

    waveHeader_[0] = { 0 };
    waveHeader_[0].dwBufferLength = waveData_[0].size() * sizeof( BufferCont::value_type );
    waveHeader_[0].dwFlags        = 0;
    waveHeader_[0].lpData         = reinterpret_cast<LPSTR>( &waveData_[0][0] );
    waveHeader_[0].dwUser         = 0UL;

    if ( ::waveInPrepareHeader( waveHandle_, &waveHeader_[0], sizeof waveHeader_[0] ) ) {
        RaiseLastOSError();
    }

    waveHeader_[1] = { 0 };
    waveHeader_[1].dwBufferLength = waveData_[1].size() * sizeof( BufferCont::value_type );
    waveHeader_[1].dwFlags        = 0;
    waveHeader_[1].lpData         = reinterpret_cast<LPSTR>( &waveData_[1][0] );
    waveHeader_[1].dwUser         = 1UL;

    if ( ::waveInPrepareHeader( waveHandle_, &waveHeader_[1], sizeof waveHeader_[1] ) ) {
        RaiseLastOSError();
    }

    if ( ::waveInAddBuffer( waveHandle_, &waveHeader_[0], sizeof waveHeader_[0] ) ) {
        RaiseLastOSError();
    }

    if ( ::waveInAddBuffer( waveHandle_, &waveHeader_[1], sizeof waveHeader_[1] ) ) {
        RaiseLastOSError();
    }

    if ( ::waveInStart( waveHandle_ ) ) {
        RaiseLastOSError();
    }
}
//---------------------------------------------------------------------------

void WaveIn::Stop()
{
    if ( !stopped_ ) {
        stopReq_ = true;
        ::WaitForSingleObject( reinterpret_cast<HANDLE>( exitEvt_->Handle ), INFINITE );
        if ( ::waveInUnprepareHeader( waveHandle_, &waveHeader_[0], sizeof waveHeader_[0] ) ) {
            RaiseLastOSError();
        }
        if ( ::waveInUnprepareHeader( waveHandle_, &waveHeader_[0], sizeof waveHeader_[0] ) ) {
            RaiseLastOSError();
        }
    }
}
//---------------------------------------------------------------------------

void WaveIn::WaveInProc( HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
                         DWORD_PTR dwParam1, DWORD_PTR dwParam2 )
{
    auto This = reinterpret_cast<WaveIn*>( dwInstance );

    switch ( uMsg ) {
        case MM_WIM_OPEN:
            break;
        case MM_WIM_DATA:
            if ( auto const Hdr = reinterpret_cast<WAVEHDR *>( dwParam1 ) ) {
                if ( This->stopReq_ ) {
                    if ( This->stopped_ ) {
                        This->exitEvt_->SetEvent();
                    }
                    else {
                        ::waveInClose( This->waveHandle_ );
                        This->stopped_ = true;
                    }
                }
                else {
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
        case MM_WIM_CLOSE:
            break;
    }
}
//---------------------------------------------------------------------------

