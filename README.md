## Cairo Experimental Video Codec
Cairo is a simple streaming video codec that was created in 2011 as part of the (now discontinued) [everyAir mobile cloud gaming project](http://www.everyair.net). Its original purpose was to help us experiment with low latency game streaming on early mobile hardware (original iPad, iPhone 3GS). Using a heavily tuned implementation, Cairo enabled everyAir to stream PC, Mac, and even PS3 games to iOS devices with extremely low encode and decode latencies.

Demo: [PS3 streaming](https://youtu.be/B14c8gFgdXM?t=64)  
Demo: [latency test](https://youtu.be/IN4wC_SVaN8?t=19)  
Demo: [everyAir](https://www.youtube.com/watch?v=amMRNjE6MsQ)

### Features

* Support for intra (i-frame) and inter (p-frame) frame prediction modes
* Motion compensation with half and quarter pixel precision
* Support for up to 16 reference frames
* Variance adaptive quantization (both uniform and non-uniform)
* Intelligent skip block detection that minimizes visual artifacts
* Periodic intra frame insertion (useful for keyframing)
* Internal support for planar YUV 4:2:0 images
* Support for multiple lossless backend compressors involving:
  * Adaptive binary arithmetic coding
  * Exponential Golomb coding
  * Run-length encoding
* Delta coded macroblocks and motion vectors
* In-loop deblocking filter
* 100% integer math
* Cross platform, fully portable code
* Simple and easy to read, designed for educational purposes

### Open Source Release
This release contains an early and *unoptimized* version of Cairo that demonstrates the basic functionality of the codec. 
Cairo's new purpose is to serve as an educational resource for students who are interested in video compression. This release presents a simple and easy to read implementation that demonstrates several common techniques without the complexities of optimizations or platform dependencies.

### Running Cairo
evx1.h describes the primary interface for Cairo. Follow these steps to create and activate the *encoder*:

* Create a bit_stream object and initialize it to some large size.
* [optional] Set your desired quality level by calling *set_quality* with a value between 1 and 31 (inclusive).
* Create an evx1_encoder object and call its *encode* method to encode a 24 bit interleaved RGB frame.

Follow these steps to create and activate the *decoder*:

* Create a bit_stream object and initialize it to some large size.
* Create an evx1_decoder object and call its *decode* method to decode a bit_stream and recover an RGB image.

### Caution: patent hazard
Note that while this release is provided under an open and permissive copyright license, the algorithms it contains are likely to be covered by existing patents that may restrict your ability to use this codec commercially. 

### More Information
For more information visit [http://www.bertolami.com](http://www.bertolami.com/index.php?engine=portfolio&content=compression&detail=cairo-video-codec).
