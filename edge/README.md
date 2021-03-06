# Edge Node Subsystem

The edge node includes the communication plane (using `ndnrtc` and `nfd`) and the computation engine (currently `YOLO`). The computation engine consumes the real-time video frames from the communication plane, runs computations (object recognizations), and publishes context features to the communication plane. 

## Pre-requisite
1. Install `Ubuntu-64bit 14.04 (amd64)`;
2. Install the following packages:

		sudo apt-get install build-essential cmake g++ python-dev autotools-dev libicu-dev build-essential libbz2-dev libboost-all-dev
		sudo apt-get install software-properties-common
		sudo apt-get install libopencv-dev python-opencv
		sudo apt-get install openssl libssl-dev
		sudo apt-get install sqlite3 libsqlite3-dev libprotobuf-dev liblog4cxx10-dev doxygen libboost-all-dev
		sudo apt-get install autotools-dev automake byacc flex binutils
		sudo add-apt-repository ppa:named-data/ppa
		sudo apt-get update
		sudo apt-get install nfd
		sudo apt-get install libconfig-dev libconfig++-dev # For building ndnrtc-client
		sudo apt-get Install libprotobuf-dev protobuf-compiler

3. In the `ndnrtc` folder, compile `ndnrtc` by following the instructions here: <https://github.com/remap/ndnrtc/blob/dev/cpp/INSTALL.md>

**NOTE 1:** In compiling the webRTC, please use the following code to generate `libwebrtc-all.mri`:

	cd /src/out/Default
	echo "create libwebrtc-all.a" > libwebrtc-all.mri
	for lib in $(find . -name '*.a'); do echo "addlib $lib" >> libwebrtc-all.mri; done;
	echo "save" >> libwebrtc-all.mri && echo "end" >> libwebrtc-all.mri
	ar -M <libwebrtc-all.mri

4. Compile `ndn-cpp` (more instructions [here](https://github.com/named-data/ndn-cpp/blob/master/INSTALL.md#ubuntu-1204-64-bit-and-32-bit-ubuntu-1404-64-bit-and-32-bit-ubuntu-1504-64-bit)):

		cd ndn-cpp	
		mkdir -p build
		./configure --with-std-shared-ptr=no --with-std-function=no --prefix=$(pwd)/build
		make && make install

5. In the `darknet` folder, compile `YOLO`:

		cd darknet
		wget https://pjreddie.com/media/files/yolo.weights
		make

6. Now it's ready to run the edge. 

	(1) Run `ndnrtc-client` consumers and producers:
	
		cd ndnrtc/cpp
		./ndnrtc-client -c ./sample-producer.cfg -p ./rule.conf -t 1800 -s CERT_FILE
		./ndnrtc-client -c ./sample-consumer.cfg -p ./rule.conf -t 1800 -s CERT_FILE

	**NOTE:** Please copy the test videos (e.g., *test-source-320x240.argb*) into `ndnrtc/cpp`.
		
	(2) Run `YOLO`:

                cd darknet
                ./darknet detector ndnrtc cfg/coco.data cfg/yolo.cfg  yolo.weights /tmp/frame_fifo -w 320 -h 240

	**NOTE:** Please change the video frame width and height accordingly using the `-w` and `-h` parameters. Please do not  change the parameter `/tmp/frame_fifo`

	(3) Run feature producer in `ndn-cpp`:

		cd ndn-cpp/bin/
		././test-annotations-example

## Known Issues

- **Performance issue in `YOLO`**: Without GPU acceleration, it takes ~10s for YOLO to process each video frame. 

- **Synchronization between `ndnrtc` and `YOLO`**: For real-time computation, ideally `YOLO` should catch up with frame fetching by `ndnrtc-client`. But without CUDA, it turns out to be impossible. Currently, `ndnrtc` and `YOLO` use a named pipe `/tmp/frame_fifo` for synchronization. But due to the limited size of pipe, if `YOLO` cannot catch up with `ndnrtc`, frame loss would be observed. 

