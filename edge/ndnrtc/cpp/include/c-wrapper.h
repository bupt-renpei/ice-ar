// 
// c-wrapper.h
//
// Copyright (c) 2017 Regents of the University of California
// For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __c_wrapper_h__
#define __c_wrapper_h__

#include "local-stream.hpp"

extern "C" {

	typedef void (*LibLog) (const char* message);

	// creates Face
	//		hostname
	//		portnum
	// creates KeyChain
	//		optional: local key storage
	// - runs everything on internal thread
	bool ndnrtc_init(const char* hostname, const char* storagePath, 
		const char* signingIdentity, const char * instanceId, LibLog libLog);

	// deinitializes library (removes connection and frees objects)
	// init can be called again after this
	void ndnrtc_deinit();


	typedef struct _LocalStreamParams {
		const char *basePrefix;
		int signingOn;
		int fecOn;
		int typeIsVideo;
		int ndnSegmentSize, ndnDataFreshnessPeriodMs;
		int frameWidth, frameHeight;
		int startBitrate, maxBitrate, gop, dropFrames;
		const char *streamName, *threadName;
	} LocalStreamParams;
	// params
	//	base prefix
	//	settings
	//		sign
	//		faceIO
	//		keychain
	//		face
	//		params
	//			stream name
	//			producer params
	//				segment size
	//				freshness
	//		coder
	//			frame_rate
	//			gop
	//			start_bitrate
	//			max_bitrate
	//			encode_height
	//			encode_width
	//			drop_frames
	//	use FEC
	ndnrtc::IStream* ndnrtc_createLocalStream(LocalStreamParams params, LibLog loggerSink);
	void ndnrtc_destroyLocalStream(ndnrtc::IStream* localStreamObject);

	void ndnrtc_LocalVideoStream_incomingI420Frame(ndnrtc::LocalVideoStream *stream,
			const unsigned int width,
			const unsigned int height,
			const unsigned int strideY,
			const unsigned int strideU,
			const unsigned int strideV,
			const unsigned char* yBuffer,
			const unsigned char* uBuffer,
			const unsigned char* vBuffer);

	const char* ndnrtc_LocalStream_getPrefix(ndnrtc::IStream *stream);
	const char* ndnrtc_LocalStream_getBasePrefix(ndnrtc::IStream *stream);
	const char* ndnrtc_LocalStream_getStreamName(ndnrtc::IStream *stream);
}

#endif