#include "eat/process/pa_init.hpp"

#include <iostream>
#include <stdexcept>

#include <portaudio.h>

class PAPPError : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

//enum class PAPPErrorCode {
//  None = paNoError,
//  NotInitialized = paNotInitialized,
//  UnanticipatedHostError = paUnanticipatedHostError,
//  InvalidChannelCount = paInvalidChannelCount,
//  InvalidSampleRate = paInvalidSampleRate,
//  InvalidDevice = paInvalidDevice,
//  InvalidFlag = paInvalidFlag,
//  SampleFormatNotSupported = paSampleFormatNotSupported,
//  BadIODeviceCombination = paBadIODeviceCombination,
//  InsufficientMemory = paInsufficientMemory,
//  BufferTooBig = paBufferTooBig,
//  BufferTooSmall = paBufferTooSmall,
//  NullCallback = paNullCallback,
//  BadStreamPtr = paBadStreamPtr,
//  TimedOut = paTimedOut,
//  InternalError = paInternalError,
//  DeviceUnavailable = paDeviceUnavailable,
//  IncompatibleHostApiSpecificStreamInfo = paIncompatibleHostApiSpecificStreamInfo,
//  StreamIsStopped = paStreamIsStopped,
//  StreamIsNotStopped = paStreamIsNotStopped,
//  InputOverflowed = paInputOverflowed,
//  OutputOverflowed = paOutputUnderflowed,
//  HostApiNotFound = paHostApiNotFound,
//  InvalidHostApi = paInvalidHostApi,
//  CanNotReadFromACallbackStream = paCanNotReadFromACallbackStream,
//  CanNotWriteToACallbackStream = paCanNotWriteToACallbackStream,
//  CanNotReadFromAnOutputOnlySteam = paCanNotReadFromAnOutputOnlyStream,
//  CanNotWriteToAnInputOnlyStream = paCanNotWriteToAnInputOnlyStream,
//  IncompatibleStreamHostApi = paIncompatibleStreamHostApi,
//  BadBufferPtr = paBadBufferPtr
//};

PAInit::PAInit() {
  auto err = Pa_Initialize();
  if(err != paNoError) {
    throw PAPPError{ Pa_GetErrorText(err)};
  }
}

PAInit::~PAInit() {
  auto err = Pa_Terminate();
  if(err != paNoError) {
    std::cerr << Pa_GetErrorText(err) << std::endl;
  }
}
