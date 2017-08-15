//
//  audio-renderer.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <webrtc/voice_engine/include/voe_network.h>
#include <webrtc/voice_engine/include/voe_base.h>
#include <webrtc/modules/audio_device/include/audio_device.h>
#include <webrtc/modules/audio_device/include/audio_device_defines.h>

#include "audio-renderer.hpp"
#include "audio-controller.hpp"

using namespace std;
using namespace ndnrtc;
using namespace webrtc;

//******************************************************************************
#pragma mark - construction/destruction
AudioRenderer::AudioRenderer(const unsigned int deviceIdx, 
  const WebrtcAudioChannel::Codec& codec):
WebrtcAudioChannel(codec),
rendering_(false)
{
    description_ = "arenderer";
    AudioController::getSharedInstance()->initVoiceEngine();

    int res = 0;
    // AudioController::getSharedInstance()->performOnAudioThread([this, deviceIdx, &res]{
      rtc::scoped_refptr<AudioDeviceModule> module = rtc::scoped_refptr<AudioDeviceModule>(AudioDeviceModule::Create(0, AudioDeviceModule::kPlatformDefaultAudio));
      int nDevices = module->PlayoutDevices();
      if (deviceIdx > nDevices)
        res = 1;
      else
        module->SetPlayoutDevice(deviceIdx);
    // });

    if (res != 0)
      throw std::runtime_error("Can't initialize audio renderer");
}

AudioRenderer::~AudioRenderer()
{
  if (rendering_)
    stopRendering();

}

//******************************************************************************
#pragma mark - public
void
AudioRenderer::startRendering()
{
  if (rendering_)
    throw std::runtime_error("Audio rendering has already started");

  int res = RESULT_OK;
  AudioController::getSharedInstance()->performOnAudioThread([this, &res](){
   // register external transport in order to playback. however, we are not
   // going to set this channel for sending and should not be getting callback
   // on webrtc::Transport callbacks
   if (voeNetwork_->RegisterExternalTransport(webrtcChannelId_, *this) < 0)
     res = voeBase_->LastError();
   else if (voeBase_->StartReceive(webrtcChannelId_) < 0)
     res = voeBase_->LastError();
   else if (voeBase_->StartPlayout(webrtcChannelId_) < 0)
     res = voeBase_->LastError();
 });

  if (RESULT_GOOD(res))
  {
    rendering_ = true;
    LogInfoC << "started" << endl;
  }
  else
  {
    stringstream ss;
    ss << "Can't start capturing due to WebRTC error " << res;
    throw std::runtime_error(ss.str());
  }
}

void
AudioRenderer::stopRendering()
{
  if (!rendering_) return;

  AudioController::getSharedInstance()->performOnAudioThread([this](){
    voeBase_->StopPlayout(webrtcChannelId_);
    voeBase_->StopReceive(webrtcChannelId_);
    voeNetwork_->DeRegisterExternalTransport(webrtcChannelId_);
  });
    
  rendering_ = false;
  LogInfoC << "stopped" << endl;
}

void
AudioRenderer::onDeliverRtpFrame(unsigned int len, unsigned char *data)
{
  if (rendering_)
    AudioController::getSharedInstance()->performOnAudioThread([this, len, data](){
        voeNetwork_->ReceivedRTPPacket(webrtcChannelId_, data, len);
    });
}

void
AudioRenderer::onDeliverRtcpFrame(unsigned int len, unsigned char *data)
{
  if (rendering_)
    AudioController::getSharedInstance()->performOnAudioThread([this, len, data](){
        voeNetwork_->ReceivedRTCPPacket(webrtcChannelId_, data, len);
    });
}
