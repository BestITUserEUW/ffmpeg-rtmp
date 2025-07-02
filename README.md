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

## Run server

```bash
./build/rtmp_server --url rtmp://127.0.0.1:8080/live
```

## Run sender

```bash
./build/rtmp_sender --url rtmp://127.0.0.1:8080/live
```