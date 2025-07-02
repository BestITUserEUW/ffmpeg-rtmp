# ffmpeg-rtmp

FFMpeg RTMP Server and Sender

## Features

- RTMP Receive Server
- RTMP Sender
- H264 Compression
- Nvidia gpu acceleration if available `nvenc` `nvdec`
- Local Dependency super build

## Build Locally

```bash
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Debug -Bbuild -H.
      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      Only needed for clangd   
```

```bash
cmake --build build -j32
```