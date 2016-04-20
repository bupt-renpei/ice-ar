//
//  video-thread.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__video_thread__
#define __ndnrtc__video_thread__

#include <boost/shared_ptr.hpp>

#include "video-coder.h"
#include "frame-data.h"

namespace ndnrtc
{
    class VideoThread : public NdnRtcComponent,
                        public IEncoderDelegate
    {
    public:
        VideoThread(const VideoCoderParams& coderParams);
        ~VideoThread();
        
        boost::shared_ptr<VideoFramePacket> encode(const WebRtcVideoFrame& frame);

        void
        setLogger(ndnlog::new_api::Logger *logger);

        unsigned int 
        getEncodedNum() { return nEncoded_; }

        unsigned int
        getDroppedNum() { return nDropped_; }

    private:
        VideoThread(const VideoThread&) = delete;
        VideoCoder coder_;
        unsigned int nEncoded_, nDropped_;
        
        #warning using shared pointer here as libstdc++ on OSX does not support std::move
        // TODO: update code to use std::move on Ubuntu
        boost::shared_ptr<VideoFramePacket> videoFramePacket_;
        
        void
        onEncodingStarted();
        
        void
        onEncodedFrame(const webrtc::EncodedImage &encodedImage);

        void
        onDroppedFrame();
    };
}

#endif /* defined(__ndnrtc__video_thread__) */
