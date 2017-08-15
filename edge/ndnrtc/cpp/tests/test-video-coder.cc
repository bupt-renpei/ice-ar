// 
// test-video-coder.cc
//
//  Created by Peter Gusev on 15 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <cstdlib>
#include <ctime>
#include <stdlib.h>
#include <boost/asio.hpp>

#include "gtest/gtest.h"
#include "src/video-coder.hpp"
#include "mock-objects/encoder-delegate-mock.hpp"
#include "tests-helpers.hpp"

// #define ENABLE_LOGGING

using namespace ::testing;
using namespace ndnrtc;
using namespace boost::chrono;

TEST(TestCoder, TestCreate)
{
	VideoCoderParams vcp(sampleVideoCoderParams());
	MockEncoderDelegate coderDelegate;
	VideoCoder *vc;

	EXPECT_NO_THROW(vc = new VideoCoder(vcp, &coderDelegate));
	// delete vc;
}

TEST(TestCoder, TestEncode)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	int width = 1280, height = 720;
	WebRtcVideoFrame convertedFrame(std::move(getFrame(width, height)));

	{ // try 1 frame
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = 1000;
		vcp.maxBitrate_ = 1000;
		vcp.encodeWidth_ = width;
		vcp.encodeHeight_ = height;
		MockEncoderDelegate coderDelegate;
		VideoCoder vc(vcp, &coderDelegate);

#ifdef ENABLE_LOGGING
		vc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

		EXPECT_CALL(coderDelegate, onEncodingStarted())
		.Times(1);
		EXPECT_CALL(coderDelegate, onEncodedFrame(_))
		.Times(1);
		EXPECT_CALL(coderDelegate, onDroppedFrame())
		.Times(0);

		vc.onRawFrame(convertedFrame);
	}

	{ // try 1 frame downscaled
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = 1000;
		vcp.maxBitrate_ = 1000;
		vcp.encodeWidth_ = width/2;
		vcp.encodeHeight_ = height/2;
		MockEncoderDelegate coderDelegate;
		VideoCoder vc(vcp, &coderDelegate);
		FrameScaler downScale(width/2, height/2);

#ifdef ENABLE_LOGGING
		vc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
		const webrtc::EncodedImage *encodedImage;
		boost::function<void(const webrtc::EncodedImage&)> onEncoded = 
			[&encodedImage](const webrtc::EncodedImage& img){
				encodedImage = &img;
			};

		EXPECT_CALL(coderDelegate, onEncodingStarted())
		.Times(1);
		EXPECT_CALL(coderDelegate, onEncodedFrame(_))
		.Times(1)
		.WillOnce(Invoke(onEncoded));
		EXPECT_CALL(coderDelegate, onDroppedFrame())
		.Times(0);

		vc.onRawFrame(downScale(convertedFrame));
		EXPECT_EQ(width/2, encodedImage->_encodedWidth);
		EXPECT_EQ(height/2, encodedImage->_encodedHeight);
	}

	{ // try 1 frame upscaled
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = 1000;
		vcp.maxBitrate_ = 1000;
		vcp.encodeWidth_ = width*2;
		vcp.encodeHeight_ = height*2;
		MockEncoderDelegate coderDelegate;
		VideoCoder vc(vcp, &coderDelegate);
		FrameScaler upScale(width*2, height*2);

#ifdef ENABLE_LOGGING
		vc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
		const webrtc::EncodedImage *encodedImage;
		boost::function<void(const webrtc::EncodedImage&)> onEncoded = 
			[&encodedImage](const webrtc::EncodedImage& img){
				encodedImage = &img;
			};

		EXPECT_CALL(coderDelegate, onEncodingStarted())
		.Times(1);
		EXPECT_CALL(coderDelegate, onEncodedFrame(_))
		.Times(1)
		.WillOnce(Invoke(onEncoded));
		EXPECT_CALL(coderDelegate, onDroppedFrame())
		.Times(0);

		vc.onRawFrame(upScale(convertedFrame));
		EXPECT_EQ(width*2, encodedImage->_encodedWidth);
		EXPECT_EQ(height*2, encodedImage->_encodedHeight);
	}

	{ // try passing wrong frame
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = 1000;
		vcp.maxBitrate_ = 1000;
		vcp.encodeWidth_ = width/2;
		vcp.encodeHeight_ = height/2;
		MockEncoderDelegate coderDelegate;
		VideoCoder vc(vcp, &coderDelegate);

#ifdef ENABLE_LOGGING
		vc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
		EXPECT_ANY_THROW(vc.onRawFrame(convertedFrame));
	}

	{ // try passing wrong frame
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = 1000;
		vcp.maxBitrate_ = 1000;
		vcp.encodeWidth_ = width*2;
		vcp.encodeHeight_ = height*2;
		MockEncoderDelegate coderDelegate;
		VideoCoder vc(vcp, &coderDelegate);

#ifdef ENABLE_LOGGING
		vc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
		EXPECT_ANY_THROW(vc.onRawFrame(convertedFrame));
	}
}
#if 1
TEST(TestCoder, TestEncode700K)
{
	bool dropEnabled = true;
	int nFrames = 30;
	int targetBitrate = 700;
	int width = 1280, height = 720;
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
	std::vector<WebRtcVideoFrame> frames = getFrameSequence(width, height, nFrames);

	{
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = targetBitrate;
		vcp.maxBitrate_ = targetBitrate;
		vcp.encodeWidth_ = width;
		vcp.encodeHeight_ = height;
		vcp.dropFramesOn_ = dropEnabled;
		MockEncoderDelegate coderDelegate;
		coderDelegate.setDefaults();
		VideoCoder vc(vcp, &coderDelegate);

#ifdef ENABLE_LOGGING
		vc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
		boost::asio::io_service io;
		boost::asio::deadline_timer runTimer(io);

		boost::chrono::duration<int, boost::milli> encodingDuration;
		high_resolution_clock::time_point encStart;

		boost::function<void()> onEncStart = [&encStart, &coderDelegate](){
			coderDelegate.countEncodingStarted();
			encStart = high_resolution_clock::now();
		};
		boost::function<void(const webrtc::EncodedImage&)> onEncEnd = [&coderDelegate, &encodingDuration, &encStart](const webrtc::EncodedImage& f){
			coderDelegate.countEncodedFrame(f);
			encodingDuration += duration_cast<milliseconds>( high_resolution_clock::now() - encStart );
		};

		EXPECT_CALL(coderDelegate, onEncodingStarted())
		.Times(nFrames)
		.WillRepeatedly(Invoke(onEncStart));
		EXPECT_CALL(coderDelegate, onEncodedFrame(_))
		.Times(AtLeast(nFrames*.1))
		.WillRepeatedly(Invoke(onEncEnd));
		EXPECT_CALL(coderDelegate, onDroppedFrame())
		.Times(AtLeast(nFrames*.8));

		high_resolution_clock::time_point t1 = high_resolution_clock::now();
		for (int i = 0; i < nFrames; ++i)
		{
			runTimer.expires_from_now(boost::posix_time::milliseconds(30));
			vc.onRawFrame(frames[i]);
			runTimer.wait();
		}
		high_resolution_clock::time_point t2 = high_resolution_clock::now();
		auto duration = duration_cast<milliseconds>( t2 - t1 ).count();
		int bitrate = (int)(((double)coderDelegate.getBytesReceived())*8/1000./(double)duration*1000);
		double avgFrameEncTimeMs = (double)encodingDuration.count()/(double)coderDelegate.getEncodedNum();

		GT_PRINTF("%d frames encoding took %d ms (avg %.2f ms per frame)\n",
			coderDelegate.getEncodedNum(), encodingDuration.count(), 
			avgFrameEncTimeMs);
		GT_PRINTF("Encoded %d (%dx%d) frames with target rate %d Kbit/s: encoded %d frames, "
			"dropped %d frames, actual rate %d Kbit/s, %d key, %d delta\n",
			nFrames, width, height, vcp.startBitrate_, coderDelegate.getEncodedNum(), 
			coderDelegate.getDroppedNum(), bitrate, coderDelegate.getKey(), coderDelegate.getDelta());
		GT_PRINTF("Encoded frame sizes (bytes): key - %.2f, delta - %.2f\n",
			coderDelegate.getAvgKeySize(), coderDelegate.getAvgDeltaSize());
		EXPECT_GE(1000./30., avgFrameEncTimeMs);
	}
}
#endif
#if 1
TEST(TestCoder, TestEncode1000K)
{
	bool dropEnabled = true;
	int nFrames = 30;
	int targetBitrate = 1000;
	int width = 1280, height = 720;
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
 	std::vector<WebRtcVideoFrame> frames = getFrameSequence(width, height, nFrames);

	{
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = targetBitrate;
		vcp.maxBitrate_ = targetBitrate;
		vcp.encodeWidth_ = width;
		vcp.encodeHeight_ = height;
		vcp.dropFramesOn_ = dropEnabled;
		MockEncoderDelegate coderDelegate;
		coderDelegate.setDefaults();
		VideoCoder vc(vcp, &coderDelegate);

#ifdef ENABLE_LOGGING
		vc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
		boost::asio::io_service io;
		boost::asio::deadline_timer runTimer(io);

		boost::chrono::duration<int, boost::milli> encodingDuration;
		high_resolution_clock::time_point encStart;

		boost::function<void()> onEncStart = [&encStart, &coderDelegate](){
			coderDelegate.countEncodingStarted();
			encStart = high_resolution_clock::now();
		};
		boost::function<void(const webrtc::EncodedImage&)> onEncEnd = [&coderDelegate, &encodingDuration, &encStart](const webrtc::EncodedImage& f){
			coderDelegate.countEncodedFrame(f);
			encodingDuration += duration_cast<milliseconds>( high_resolution_clock::now() - encStart );
		};

		EXPECT_CALL(coderDelegate, onEncodingStarted())
		.Times(nFrames)
		.WillRepeatedly(Invoke(onEncStart));
		EXPECT_CALL(coderDelegate, onEncodedFrame(_))
		.Times(AtLeast(nFrames*.15))
		.WillRepeatedly(Invoke(onEncEnd));
		EXPECT_CALL(coderDelegate, onDroppedFrame())
		.Times(AtLeast(nFrames*.75));

		high_resolution_clock::time_point t1 = high_resolution_clock::now();
		for (int i = 0; i < nFrames; ++i)
		{
			runTimer.expires_from_now(boost::posix_time::milliseconds(30));
			vc.onRawFrame(frames[i]);
			runTimer.wait();
		}
		high_resolution_clock::time_point t2 = high_resolution_clock::now();
		auto duration = duration_cast<milliseconds>( t2 - t1 ).count();
		int bitrate = (int)(((double)coderDelegate.getBytesReceived())*8/1000./(double)duration*1000);
		double avgFrameEncTimeMs = (double)encodingDuration.count()/(double)coderDelegate.getEncodedNum();

		GT_PRINTF("%d frames encoding took %d ms (avg %.2f ms per frame)\n",
			coderDelegate.getEncodedNum(), encodingDuration.count(), 
			avgFrameEncTimeMs);
		GT_PRINTF("Encoded %d (%dx%d) frames with target rate %d Kbit/s: encoded %d frames, "
			"dropped %d frames, actual rate %d Kbit/s, %d key, %d delta\n",
			nFrames, width, height, vcp.startBitrate_, coderDelegate.getEncodedNum(), 
			coderDelegate.getDroppedNum(), bitrate, coderDelegate.getKey(), coderDelegate.getDelta());
		GT_PRINTF("Encoded frame sizes (bytes): key - %.2f, delta - %.2f\n",
			coderDelegate.getAvgKeySize(), coderDelegate.getAvgDeltaSize());
		EXPECT_GE(1000./30., avgFrameEncTimeMs);
	}
}
#endif
#if 1
TEST(TestCoder, TestEncode2000K)
{
	bool dropEnabled = true;
	int nFrames = 30;
	int targetBitrate = 2000;
	int width = 1280, height = 720;
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
 	std::vector<WebRtcVideoFrame> frames = getFrameSequence(width, height, nFrames);

	{
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = targetBitrate;
		vcp.maxBitrate_ = targetBitrate;
		vcp.encodeWidth_ = width;
		vcp.encodeHeight_ = height;
		vcp.dropFramesOn_ = dropEnabled;
		MockEncoderDelegate coderDelegate;
		coderDelegate.setDefaults();
		VideoCoder vc(vcp, &coderDelegate);

#ifdef ENABLE_LOGGING
		vc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
		boost::asio::io_service io;
		boost::asio::deadline_timer runTimer(io);
		boost::chrono::duration<int, boost::milli> encodingDuration;
		high_resolution_clock::time_point encStart;

		boost::function<void()> onEncStart = [&encStart, &coderDelegate](){
			coderDelegate.countEncodingStarted();
			encStart = high_resolution_clock::now();
		};
		boost::function<void(const webrtc::EncodedImage&)> onEncEnd = [&coderDelegate, &encodingDuration, &encStart](const webrtc::EncodedImage& f){
			coderDelegate.countEncodedFrame(f);
			encodingDuration += duration_cast<milliseconds>( high_resolution_clock::now() - encStart );
		};

		EXPECT_CALL(coderDelegate, onEncodingStarted())
		.Times(nFrames)
		.WillRepeatedly(Invoke(onEncStart));
		EXPECT_CALL(coderDelegate, onEncodedFrame(_))
		.Times(AtLeast(nFrames*.2))
		.WillRepeatedly(Invoke(onEncEnd));
		EXPECT_CALL(coderDelegate, onDroppedFrame())
		.Times(AtLeast(nFrames*.5));

		high_resolution_clock::time_point t1 = high_resolution_clock::now();
		for (int i = 0; i < nFrames; ++i)
		{
			runTimer.expires_from_now(boost::posix_time::milliseconds(30));
			vc.onRawFrame(frames[i]);
			runTimer.wait();
		}
		high_resolution_clock::time_point t2 = high_resolution_clock::now();
		auto duration = duration_cast<milliseconds>( t2 - t1 ).count();
		int bitrate = (int)(((double)coderDelegate.getBytesReceived())*8/1000./(double)duration*1000);
		double avgFrameEncTimeMs = (double)encodingDuration.count()/(double)coderDelegate.getEncodedNum();

		GT_PRINTF("%d frames encoding took %d ms (avg %.2f ms per frame)\n",
			coderDelegate.getEncodedNum(), encodingDuration.count(), 
			avgFrameEncTimeMs);
		GT_PRINTF("Encoded %d (%dx%d) frames with target rate %d Kbit/s: encoded %d frames, "
			"dropped %d frames, actual rate %d Kbit/s, %d key, %d delta\n",
			nFrames, width, height, vcp.startBitrate_, coderDelegate.getEncodedNum(), 
			coderDelegate.getDroppedNum(), bitrate, coderDelegate.getKey(), coderDelegate.getDelta());
		GT_PRINTF("Encoded frame sizes (bytes): key - %.2f, delta - %.2f\n",
			coderDelegate.getAvgKeySize(), coderDelegate.getAvgDeltaSize());
		EXPECT_GE(1000./30., avgFrameEncTimeMs);
	}
}
#endif
#if 1
TEST(TestCoder, TestEncode3000K)
{
	bool dropEnabled = true;
	int nFrames = 30;
	int targetBitrate = 3000;
	int width = 1280, height = 720;
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
	std::vector<WebRtcVideoFrame> frames = getFrameSequence(width, height, nFrames);

	{
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = targetBitrate;
		vcp.maxBitrate_ = targetBitrate;
		vcp.encodeWidth_ = width;
		vcp.encodeHeight_ = height;
		vcp.dropFramesOn_ = dropEnabled;
		MockEncoderDelegate coderDelegate;
		coderDelegate.setDefaults();
		VideoCoder vc(vcp, &coderDelegate);

#ifdef ENABLE_LOGGING
		vc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
		boost::asio::io_service io;
		boost::asio::deadline_timer runTimer(io);
		boost::chrono::duration<int, boost::milli> encodingDuration;
		high_resolution_clock::time_point encStart;

		boost::function<void()> onEncStart = [&encStart, &coderDelegate](){
			coderDelegate.countEncodingStarted();
			encStart = high_resolution_clock::now();
		};
		boost::function<void(const webrtc::EncodedImage&)> onEncEnd = [&coderDelegate, &encodingDuration, &encStart](const webrtc::EncodedImage& f){
			coderDelegate.countEncodedFrame(f);
			encodingDuration += duration_cast<milliseconds>( high_resolution_clock::now() - encStart );
		};

		EXPECT_CALL(coderDelegate, onEncodingStarted())
		.Times(nFrames)
		.WillRepeatedly(Invoke(onEncStart));
		EXPECT_CALL(coderDelegate, onEncodedFrame(_))
		.Times(AtLeast(nFrames*.3))
		.WillRepeatedly(Invoke(onEncEnd));
		EXPECT_CALL(coderDelegate, onDroppedFrame())
		.Times(AtLeast(nFrames*.4));

		high_resolution_clock::time_point t1 = high_resolution_clock::now();
		for (int i = 0; i < nFrames; ++i)
		{
			runTimer.expires_from_now(boost::posix_time::milliseconds(30));
			vc.onRawFrame(frames[i]);
			runTimer.wait();
		}
		high_resolution_clock::time_point t2 = high_resolution_clock::now();
		auto duration = duration_cast<milliseconds>( t2 - t1 ).count();
		int bitrate = (int)(((double)coderDelegate.getBytesReceived())*8/1000./(double)duration*1000);
		double avgFrameEncTimeMs = (double)encodingDuration.count()/(double)coderDelegate.getEncodedNum();

		GT_PRINTF("%d frames encoding took %d ms (avg %.2f ms per frame)\n",
			coderDelegate.getEncodedNum(), encodingDuration.count(), 
			avgFrameEncTimeMs);
		GT_PRINTF("Encoded %d (%dx%d) frames with target rate %d Kbit/s: encoded %d frames, "
			"dropped %d frames, actual rate %d Kbit/s, %d key, %d delta\n",
			nFrames, width, height, vcp.startBitrate_, coderDelegate.getEncodedNum(), 
			coderDelegate.getDroppedNum(), bitrate, coderDelegate.getKey(), coderDelegate.getDelta());
		GT_PRINTF("Encoded frame sizes (bytes): key - %.2f, delta - %.2f\n",
			coderDelegate.getAvgKeySize(), coderDelegate.getAvgDeltaSize());
		EXPECT_GE(1000./30., avgFrameEncTimeMs);
	}
}
#endif
#if 1
TEST(TestCoder, Test720pEnforceNoDrop)
{
    bool dropEnabled = false;
    int nFrames = 30;
    int targetBitrate = 1000;
    int width = 1280, height = 720;
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
	std::vector<WebRtcVideoFrame> frames = getFrameSequence(width, height, nFrames);
    
    {
        VideoCoderParams vcp(sampleVideoCoderParams());
        vcp.startBitrate_ = targetBitrate;
        vcp.maxBitrate_ = targetBitrate;
        vcp.encodeWidth_ = width;
        vcp.encodeHeight_ = height;
        vcp.dropFramesOn_ = dropEnabled;
        MockEncoderDelegate coderDelegate;
        coderDelegate.setDefaults();
        VideoCoder vc(vcp, &coderDelegate);
        
#ifdef ENABLE_LOGGING
        vc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
        boost::asio::io_service io;
        boost::asio::deadline_timer runTimer(io);
        boost::chrono::duration<int, boost::milli> encodingDuration;
        high_resolution_clock::time_point encStart;
        
        boost::function<void()> onEncStart = [&encStart, &coderDelegate](){
            coderDelegate.countEncodingStarted();
            encStart = high_resolution_clock::now();
        };
        boost::function<void(const webrtc::EncodedImage&)> onEncEnd = [&coderDelegate, &encodingDuration, &encStart](const webrtc::EncodedImage& f){
            coderDelegate.countEncodedFrame(f);
            encodingDuration += duration_cast<milliseconds>( high_resolution_clock::now() - encStart );
        };
        
        EXPECT_CALL(coderDelegate, onEncodingStarted())
        .Times(nFrames)
        .WillRepeatedly(Invoke(onEncStart));
        EXPECT_CALL(coderDelegate, onEncodedFrame(_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(onEncEnd));
        EXPECT_CALL(coderDelegate, onDroppedFrame())
        .Times(0);
        
        high_resolution_clock::time_point t1 = high_resolution_clock::now();
        for (int i = 0; i < nFrames; ++i)
        {
            runTimer.expires_from_now(boost::posix_time::milliseconds(30));
            vc.onRawFrame(frames[i]);
            runTimer.wait();
        }
        high_resolution_clock::time_point t2 = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>( t2 - t1 ).count();
        int bitrate = (int)(((double)coderDelegate.getBytesReceived())*8/1000./(double)duration*1000);
        double avgFrameEncTimeMs = (double)encodingDuration.count()/(double)coderDelegate.getEncodedNum();
        
        GT_PRINTF("%d frames encoding took %d ms (avg %.2f ms per frame)\n",
                  coderDelegate.getEncodedNum(), encodingDuration.count(),
                  avgFrameEncTimeMs);
        GT_PRINTF("Encoded %d (%dx%d) frames with target rate %d Kbit/s: encoded %d frames, "
                  "dropped %d frames, actual rate %d Kbit/s, %d key, %d delta\n",
                  nFrames, width, height, vcp.startBitrate_, coderDelegate.getEncodedNum(), 
                  coderDelegate.getDroppedNum(), bitrate, coderDelegate.getKey(), coderDelegate.getDelta());
        GT_PRINTF("Encoded frame sizes (bytes): key - %.2f, delta - %.2f\n",
                  coderDelegate.getAvgKeySize(), coderDelegate.getAvgDeltaSize());
        EXPECT_GE(1000./30., avgFrameEncTimeMs);
    }
}
#endif
#if 1
TEST(TestCoder, TestEnforceNoDrop)
{
	bool dropEnabled = false;
	int nFrames = 30;
	int targetBitrate = 1000;
	int width = 1920, height = 1080;
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
	std::vector<WebRtcVideoFrame> frames = getFrameSequence(width, height, nFrames);

	{
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = targetBitrate;
		vcp.maxBitrate_ = targetBitrate;
		vcp.encodeWidth_ = width;
		vcp.encodeHeight_ = height;
		vcp.dropFramesOn_ = dropEnabled;
		MockEncoderDelegate coderDelegate;
		coderDelegate.setDefaults();
		VideoCoder vc(vcp, &coderDelegate);

#ifdef ENABLE_LOGGING
		vc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
		boost::asio::io_service io;
		boost::asio::deadline_timer runTimer(io);
		boost::chrono::duration<int, boost::milli> encodingDuration;
		high_resolution_clock::time_point encStart;

		boost::function<void()> onEncStart = [&encStart, &coderDelegate](){
			coderDelegate.countEncodingStarted();
			encStart = high_resolution_clock::now();
		};
		boost::function<void(const webrtc::EncodedImage&)> onEncEnd = [&coderDelegate, &encodingDuration, &encStart](const webrtc::EncodedImage& f){
			coderDelegate.countEncodedFrame(f);
			encodingDuration += duration_cast<milliseconds>( high_resolution_clock::now() - encStart );
		};

		EXPECT_CALL(coderDelegate, onEncodingStarted())
		.Times(nFrames)
		.WillRepeatedly(Invoke(onEncStart));
		EXPECT_CALL(coderDelegate, onEncodedFrame(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onEncEnd));
		EXPECT_CALL(coderDelegate, onDroppedFrame())
		.Times(0);

		high_resolution_clock::time_point t1 = high_resolution_clock::now();
		for (int i = 0; i < nFrames; ++i)
		{
			runTimer.expires_from_now(boost::posix_time::milliseconds(30));
			vc.onRawFrame(frames[i]);
			runTimer.wait();
		}
		high_resolution_clock::time_point t2 = high_resolution_clock::now();
		auto duration = duration_cast<milliseconds>( t2 - t1 ).count();
		int bitrate = (int)(((double)coderDelegate.getBytesReceived())*8/1000./(double)duration*1000);
		double avgFrameEncTimeMs = (double)encodingDuration.count()/(double)coderDelegate.getEncodedNum();

		GT_PRINTF("%d frames encoding took %d ms (avg %.2f ms per frame)\n",
			coderDelegate.getEncodedNum(), encodingDuration.count(), 
			avgFrameEncTimeMs);
		GT_PRINTF("Encoded %d (%dx%d) frames with target rate %d Kbit/s: encoded %d frames, "
			"dropped %d frames, actual rate %d Kbit/s, %d key, %d delta\n",
			nFrames, width, height, vcp.startBitrate_, coderDelegate.getEncodedNum(), 
			coderDelegate.getDroppedNum(), bitrate, coderDelegate.getKey(), coderDelegate.getDelta());
		GT_PRINTF("Encoded frame sizes (bytes): key - %.2f, delta - %.2f\n",
			coderDelegate.getAvgKeySize(), coderDelegate.getAvgDeltaSize());
		EXPECT_GE(1000./30., avgFrameEncTimeMs);
	}
}
#endif
#if 1
TEST(TestCoder, TestEnforceKeyGop)
{
	bool dropEnabled = true;
	int nFrames = 90;
	int targetBitrate = 10000;
	int width = 1280, height = 720;
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
	std::vector<WebRtcVideoFrame> frames = getFrameSequence(width, height, nFrames);

	{
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = targetBitrate;
		vcp.maxBitrate_ = targetBitrate;
		vcp.encodeWidth_ = width;
		vcp.encodeHeight_ = height;
		vcp.dropFramesOn_ = dropEnabled;
		MockEncoderDelegate coderDelegate;
		coderDelegate.setDefaults();
		VideoCoder vc(vcp, &coderDelegate, VideoCoder::KeyEnforcement::Gop);

#ifdef ENABLE_LOGGING
		vc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
		boost::asio::io_service io;
		boost::asio::deadline_timer runTimer(io);

		boost::chrono::duration<int, boost::milli> encodingDuration;
		high_resolution_clock::time_point encStart;

		boost::function<void()> onEncStart = [&encStart, &coderDelegate](){
			coderDelegate.countEncodingStarted();
			encStart = high_resolution_clock::now();
		};
		boost::function<void(const webrtc::EncodedImage&)> onEncEnd = [&coderDelegate, &encodingDuration, &encStart](const webrtc::EncodedImage& f){
			coderDelegate.countEncodedFrame(f);
			encodingDuration += duration_cast<milliseconds>( high_resolution_clock::now() - encStart );
		};

		EXPECT_CALL(coderDelegate, onEncodingStarted())
		.Times(nFrames)
		.WillRepeatedly(Invoke(onEncStart));
		EXPECT_CALL(coderDelegate, onEncodedFrame(_))
		.Times(nFrames)
		.WillRepeatedly(Invoke(onEncEnd));
		EXPECT_CALL(coderDelegate, onDroppedFrame())
		.Times(0);

		high_resolution_clock::time_point t1 = high_resolution_clock::now();
		for (int i = 0; i < nFrames; ++i)
		{
			runTimer.expires_from_now(boost::posix_time::milliseconds(30));
			vc.onRawFrame(frames[i]);
			runTimer.wait();
		}
		high_resolution_clock::time_point t2 = high_resolution_clock::now();
		auto duration = duration_cast<milliseconds>( t2 - t1 ).count();
		int bitrate = (int)(((double)coderDelegate.getBytesReceived())*8/1000./(double)duration*1000);
		double avgFrameEncTimeMs = (double)encodingDuration.count()/(double)coderDelegate.getEncodedNum();

		GT_PRINTF("%d frames encoding took %d ms (avg %.2f ms per frame)\n",
			coderDelegate.getEncodedNum(), encodingDuration.count(), avgFrameEncTimeMs);
		GT_PRINTF("Encoded %d (%dx%d) frames with target rate %d Kbit/s: encoded %d frames, "
			"dropped %d frames, actual rate %d Kbit/s, %d key, %d delta\n",
			nFrames, width, height, vcp.startBitrate_, coderDelegate.getEncodedNum(), 
			coderDelegate.getDroppedNum(), bitrate, coderDelegate.getKey(), coderDelegate.getDelta());
		GT_PRINTF("Encoded frame sizes (bytes): key - %.2f, delta - %.2f\n",
			coderDelegate.getAvgKeySize(), coderDelegate.getAvgDeltaSize());

		EXPECT_GE(1000./30., avgFrameEncTimeMs);
		EXPECT_EQ(nFrames/vcp.gop_, coderDelegate.getKey());
		EXPECT_EQ(nFrames-nFrames/vcp.gop_, coderDelegate.getDelta());
	}
}
#endif
#if 1
TEST(TestCoder, TestEnforceKeyTimed)
{
	bool dropEnabled = true;
	int nFrames = 90;
	int targetBitrate = 3000;
	int width = 1280, height = 720;
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
	std::vector<WebRtcVideoFrame> frames = getFrameSequence(width, height, nFrames);

	{
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = targetBitrate;
		vcp.maxBitrate_ = targetBitrate;
		vcp.encodeWidth_ = width;
		vcp.encodeHeight_ = height;
		vcp.dropFramesOn_ = dropEnabled;
		MockEncoderDelegate coderDelegate;
		coderDelegate.setDefaults();
		VideoCoder vc(vcp, &coderDelegate, VideoCoder::KeyEnforcement::Timed);

#ifdef ENABLE_LOGGING
		vc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
		boost::asio::io_service io;
		boost::asio::deadline_timer runTimer(io);

		boost::chrono::duration<int, boost::milli> encodingDuration;
		high_resolution_clock::time_point encStart;

		boost::function<void()> onEncStart = [&encStart, &coderDelegate](){
			coderDelegate.countEncodingStarted();
			encStart = high_resolution_clock::now();
		};
		boost::function<void(const webrtc::EncodedImage&)> onEncEnd = [&coderDelegate, &encodingDuration, &encStart](const webrtc::EncodedImage& f){
			coderDelegate.countEncodedFrame(f);
			encodingDuration += duration_cast<milliseconds>( high_resolution_clock::now() - encStart );
		};

		EXPECT_CALL(coderDelegate, onEncodingStarted())
		.Times(nFrames)
		.WillRepeatedly(Invoke(onEncStart));
		EXPECT_CALL(coderDelegate, onEncodedFrame(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onEncEnd));
		EXPECT_CALL(coderDelegate, onDroppedFrame())
		.Times(AtLeast(1));

		high_resolution_clock::time_point t1 = high_resolution_clock::now();
		for (int i = 0; i < nFrames; ++i)
		{
			runTimer.expires_from_now(boost::posix_time::milliseconds(30));
			vc.onRawFrame(frames[i]);
			runTimer.wait();
		}
		high_resolution_clock::time_point t2 = high_resolution_clock::now();
		auto duration = duration_cast<milliseconds>( t2 - t1 ).count();
		int bitrate = (int)(((double)coderDelegate.getBytesReceived())*8/1000./(double)duration*1000);
		double avgFrameEncTimeMs = (double)encodingDuration.count()/(double)coderDelegate.getEncodedNum();

		GT_PRINTF("%d frames encoding took %d ms (avg %.2f ms per frame)\n",
			coderDelegate.getEncodedNum(), encodingDuration.count(), 
			avgFrameEncTimeMs);
		GT_PRINTF("Encoded %d (%dx%d) frames with target rate %d Kbit/s: encoded %d frames, "
			"dropped %d frames, actual rate %d Kbit/s, %d key, %d delta\n",
			nFrames, width, height, vcp.startBitrate_, coderDelegate.getEncodedNum(), 
			coderDelegate.getDroppedNum(), bitrate, coderDelegate.getKey(), coderDelegate.getDelta());
		GT_PRINTF("Encoded frame sizes (bytes): key - %.2f, delta - %.2f\n",
			coderDelegate.getAvgKeySize(), coderDelegate.getAvgDeltaSize());

		EXPECT_EQ(nFrames/vcp.gop_, coderDelegate.getKey());
		EXPECT_EQ(coderDelegate.getEncodedNum()-nFrames/vcp.gop_, coderDelegate.getDelta());
	}
}
#endif

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
