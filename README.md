# 录屏软件

## 简介

本项目是为了学习FFmpeg开发使用，使用FFmpeg多个库文件(如libavcodec, libavformat, libswresample, libavfilter)中的函数开发录屏软件，同时了解音视频采集、编码、封装的过程。

## 内容

### 一、录制桌面视频

#### 1. 打开gdigrab

使用FFmpeg gdi录制桌面。打开音视频输入文件的函数为

```c
int avformat_open_input(AVFormatContext **ps, const char *url, ff_const59 AVInputFormat *fmt, AVDictionary **options);
```

其中`url`参数可以是文件名，也可以是流的名称。设置`url`参数为`"desktop"`，设置`fmt`参数为`av_find_input_format("gdigrab")`获得，打开gdigrab。通过`options`参数设置录屏的区域坐标。

#### 2. 解析输入流中的视频信息

使用`int avformat_find_stream_info(AVFormatContext *ic, AVDictionary **options)`函数解析输入文件的媒体流信息，该函数做的工作主要是遍历输入音视频文件中的各路媒体流，读取各路媒体流的信息（如解码器信息），将读取到的流信息保存到输入参数`AVFormatContext *ic`的`ic->streams`成员中。

#### 3. 解码录屏视频流

上一步解析视频流的信息保存到了`AVFormatContext`的`streams`成员中，而`stream`的`AVCodecContext *codec`成员包含了解码器相关信息。通过如下代码查找到解码器。

```c
codec_ctx_ = fmt_ctx_->streams[stream_index]->codec;
codec_ = avcodec_find_decoder(codec_ctx_->codec_id);
```

通过`int avcodec_open2(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options)`函数对解码器上下文`avctx`进行初始化。该函数给`AVCodecContext`内部的数据成员分配内存空间，进行解码参数的校验。

初始化解码相关参数后，将开启桌面图像录制线程。通过`int av_read_frame(AVFormatContext *s, AVPacket *pkt)`读取一帧录取并压缩后的数据，通过`AVPacket`结构中的`stream_index`成员判断这是不是压缩后的视频帧，如果是是一所后的视频帧，通过`int avcodec_send_packet(AVCodecContext *avctx, const AVPacket *avpkt)`将该帧数据送入解码器中，然后通过`int avcodec_receive_frame(AVCodecContext *avctx, AVFrame *frame)`读取解码器解码后的视频帧，将该帧存到队列中，等待后续编码线程从该队列中取出并编码。录屏线程和后续的编码线程实际是”生产者与消费者“的关系。

### 二、录制麦克风和扬声器音频

通过FFmpeg对virtual-audio-capturer进行捕获，需要安装[screen-capture](https://sourceforge.net/projects/screencapturer/)。录制麦克风扬声器音频的流程和调用的函数和录制桌面视频大致一样。

### 三、视频格式转换

录屏得到的视频流时RGBA格式的，而H.264编码器的输入格式为YUV，所以编码录屏视频流前，需要对视频流进行格式转换。

格式转换主要利用了`libswscale`库中的函数，首先初始化`SwsContext`上下文信息，其中保存了输入图像和输出图像的宽高、像素格式等参数，然后调用`sws_scale`函数进行转换。

初始化`SwsContext`用了如下函数，函数的`flags`参数指定图像在缩放时使用的采样算法或插值算法

```c++
struct SwsContext *sws_getContext(int srcW, int srcH, enum AVPixelFormat srcFormat,
                                  int dstW, int dstH, enum AVPixelFormat dstFormat,
                                  int flags, SwsFilter *srcFilter,
                                  SwsFilter *dstFilter, const double *param);
```

`SwsContext`函数声明如下

```c++
int sws_scale(struct SwsContext *c, const uint8_t *const srcSlice[],
              const int srcStride[], int srcSliceY, int srcSliceH,
              uint8_t *const dst[], const int dstStride[]);
```

其中`srcSlice`为一个数组，这个数组保存了图像每个缓存通道的地址，类似于`AVFrame`的`data`成员。`srcStride`为一个数组，这个数组记录了输入源图像的每个缓存通道的长度，类似于`AVFrame`的`linesize`成员。`srcSliceY`为输入源图像数据在通道中的起始位置偏移量。`srcSliceH`为输入源图像的通道个数。`dst`为输出目标图像的缓存地址数组，`dstStride`为输出目标图像的缓存宽度数组。

### 四、视频编码























































































