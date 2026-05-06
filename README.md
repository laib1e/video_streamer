# video_streamer

`video_streamer` — проект видеотрансляции с Raspberry Pi CSI camera: захват кадров с камеры, кодирование в H.264, упаковка H.264 NAL units в RTP и управление клиентом через RTSP.

Проект embedded/video pipeline на C++20 с упором на системную разработку под Linux: POSIX sockets, `epoll`, UDP/RTP, RTSP, FFmpeg/libavcodec, lock-free SPSC очередь и разделение media pipeline на независимые слои.

## Текущий статус

Рабочая версия проекта умеет:

- захватывать кадры с Raspberry Pi camera через V4L2 compatibility layer;
- кодировать raw frames в H.264 через FFmpeg/libavcodec;
- разбирать H.264 Annex B stream на NAL units;
- отправлять H.264 по RTP/UDP:
  - small NAL units — как Single NAL Unit Packet;
  - large NAL units — через FU-A fragmentation;
- отдавать RTSP control plane:
  - `OPTIONS`;
  - `DESCRIBE`;
  - `SETUP`;
  - `PLAY`;
  - `TEARDOWN`;
- воспроизводить поток в VLC по RTSP URL;
- передавать поток на ASUSTOR NAS, где `ffmpeg` забирает RTSP stream, перекладывает его в HLS и отдаёт через `nginx`;
- публиковать web-страницу трансляции через reverse proxy.

## Demo-сценарий

Основной демонстрационный сценарий:

```text
Raspberry Pi CSI Camera
    ↓
video_streamer
    ↓ RTSP/RTP
ASUSTOR NAS
    ↓ ffmpeg RTSP client
HLS segments
    ↓ nginx
Web page
    ↓ reverse proxy
Browser / VLC client
```

Для удалённой демонстрации внешний клиент проходит через controlled network access / port knocking, после чего открывает web-страницу трансляции через reverse proxy.

Такой вариант выбран потому, что чистый RTSP/RTP через интернет сложнее демонстрировать надёжно: RTSP control plane идёт по TCP, а RTP media обычно идёт отдельным UDP-потоком. HLS поверх HTTP проще проксировать и открывать в браузере.

## Архитектура проекта

Упрощённая схема v1:

```text
Camera
  ↓ raw Frame
LockFreeQueue<Frame, 64>
  ↓
VideoStreamer
  ├── H264Encoder
  ├── H264AnnexBParser
  ├── H264 RTP packetization
  └── RTPServer / UDP sender

RTSPServer
  └── control plane:
      OPTIONS / DESCRIBE / SETUP / PLAY / TEARDOWN
```

Более подробно:

```text
CameraSource
    ↓
LockFreeQueue<Frame, 64>
    ↓
VideoStreamer
    ↓
H264Encoder
    ↓
H.264 Annex B byte stream
    ↓
H264AnnexBParser
    ↓
NAL units
    ↓
H264 RTP packetization
    ↓
RTPServer
    ↓
RTP/UDP packets

RTSPServer
    ├── accepts RTSP TCP clients
    ├── returns SDP on DESCRIBE
    ├── parses Transport header on SETUP
    ├── configures RTP destination
    └── starts/stops streaming on PLAY/TEARDOWN
```

В проекте намеренно разделены два слоя:

- **Data plane** — захват, кодирование, RTP packetization, UDP delivery.
- **Control plane** — RTSP-сервер, SDP, SETUP/PLAY/TEARDOWN.

RTSP-сервер не кодирует видео и не разбирает H.264. Он только сообщает клиенту, какой поток доступен, узнаёт клиентский UDP-порт и управляет состоянием трансляции.

## Основные компоненты

### LockFreeQueue

`LockFreeQueue<Frame, 64>` — bounded SPSC очередь между capture-потоком и streaming-потоком.

Зачем она нужна:

- camera capture не должен блокироваться сетевой отправкой;
- encoder/RTP path работает как consumer;
- очередь ограничивает накопление задержки;
- для live video важнее актуальность кадра, чем бесконечное накопление старых кадров.

### CameraSource

Текущая v1 использует V4L2 compatibility layer поверх libcamera.

Ограничение текущей версии: на Raspberry Pi с CSI camera через V4L2 compatibility layer наблюдается нестабильный frame pacing и просадка фактического FPS. Это одна из причин, почему в v2 planned native `libcamera` backend.

### H264Encoder

`H264Encoder` отвечает только за кодирование YUYV420 в H.264.

Он не знает про:

- RTP;
- UDP;
- RTSP;
- клиентов;
- socket’ы.

Типовой pipeline encoder’а:

```text
Frame from CameraSource
    ↓
pixel format conversion
    ↓
AVFrame
    ↓ avcodec_send_frame()
FFmpeg H.264 encoder
    ↓ avcodec_receive_packet()
AVPacket with H.264 Annex B data
```

Важные детали:

- `AVFrame` — raw uncompressed video frame.
- `AVPacket` — compressed H.264 data.
- `libswscale` используется для pixel format conversion.
- `libavcodec` выполняет H.264 encoding.
- encoder работает через send/receive API: один input frame не обязан немедленно давать ровно один output packet.
- H.264 output содержит NAL units: SPS, PPS, IDR, non-IDR slices.

Для low-latency streaming используются настройки без B-frames, с коротким GOP и периодическими keyframes/IDR.

### H264AnnexBParser

`H264AnnexBParser` выделяет NAL units из H.264 Annex B byte stream.

Annex B использует start codes:

```text
00 00 01
00 00 00 01
```

Пример:

```text
00 00 00 01 [SPS]
00 00 00 01 [PPS]
00 00 00 01 [IDR]
```

Parser отдаёт NAL units без start code:

```text
[SPS]
[PPS]
[IDR]
```

Это нужно потому, что RTP payload для H.264 работает с NAL units, а не с Annex B start codes.

### RTPServer / H.264 RTP packetization

RTP delivery использует RTP header 12 bytes:

- RTP version: 2;
- payload type: 96 dynamic;
- H.264 RTP clock: 90 kHz;
- sequence number увеличивается на каждый RTP packet;
- timestamp относится к media time кадра/access unit;
- marker bit ставится на последний RTP packet access unit.

H.264 packetization:

```text
Small NAL unit:
    RTP header + complete NAL unit

Large NAL unit:
    RTP header + FU-A indicator + FU-A header + NAL fragment
```

FU-A используется для больших NAL units, которые не помещаются в безопасный RTP payload size.

Важно:

- sequence number увеличивается на каждый RTP packet;
- все RTP packets одного access unit имеют одинаковый RTP timestamp;
- marker bit выставляется только на последнем RTP packet access unit;
- RTP timestamp теперь считается от реального времени кадра, а не фиксированным шагом от номинального FPS.

## Исправление RTP timestamp

В ранней версии RTP timestamp увеличивался фиксированным шагом:

```text
timestamp += 90000 / fps
```

Это работало только если реальный FPS совпадает с номинальным FPS. На Raspberry Pi через V4L2 compatibility layer фактический FPS оказался ниже и с неровным pacing. VLC это частично терпел, но HLS pipeline ломался.

Текущая модель:

```text
capture timestamp / frame timestamp
    ↓
elapsed_us
    ↓
rtp_timestamp = initial_timestamp + elapsed_us * 90000 / 1'000'000
```

Почему это важно:

- RTP timestamp должен описывать media time, а не количество обработанных кадров;
- HLS segmenter чувствителен к некорректной временной шкале;
- все RTP packets одного access unit получают одинаковый timestamp;
- marker bit указывает конец access unit.

После перехода на timestamp от реального времени кадра RTSP → HLS pipeline стал стабильнее.

## RTSPServer

RTSP реализован как TCP control plane.

Поддерживаемые методы:

```text
OPTIONS
DESCRIBE
SETUP
PLAY
TEARDOWN
```

Пример последовательности:

```text
Client → OPTIONS
Server → supported methods

Client → DESCRIBE
Server → SDP

Client → SETUP
Server → parse Transport header, client_port

Client → PLAY
Server → start RTP delivery

Client → TEARDOWN
Server → stop session
```

`DESCRIBE` возвращает SDP:

```text
v=0
o=- 0 0 IN IP4 0.0.0.0
s=video_streamer
c=IN IP4 0.0.0.0
t=0 0
a=control:*
m=video 0 RTP/AVP 96
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1
a=control:trackID=0
```

Строка `c=IN IP4 0.0.0.0` важна для совместимости с VLC/LIVE555.

`SETUP` обрабатывает Transport header:

```text
Transport: RTP/AVP;unicast;client_port=65320-65321
```

Сервер извлекает:

```text
client RTP port  = 65320
client RTCP port = 65321
```

После этого RTP sender настраивает destination:

```text
client_ip:client_rtp_port
```

## HLS / web demo

Для внешней демонстрации проект используется в связке с отдельным сервером:

```text
video_streamer on Raspberry Pi
    ↓ RTSP/RTP
ffmpeg
    ↓
HLS playlist + .ts/.m4s segments
    ↓
nginx
    ↓
web page / reverse proxy
```

Схема удобна для удалённого доступа:

- внешнему клиенту не нужно напрямую принимать RTP/UDP;
- web-страница доступна через reverse proxy;
- доступ во внутреннюю сеть открывается контролируемо через port knocking/firewall;
- HLS воспроизводится в браузере через HTTPS.

RTSP → HLS преобразование выполняется на сервере в Docker. Используются два контейнера:

- `video-demo-ffmpeg` забирает RTSP-поток с Raspberry Pi, перекодирует его в H.264 с low-latency настройками и формирует HLS playlist/segments;
- `video-demo-nginx` отдаёт статическую web-страницу и HLS-файлы.

Пример `docker-compose.yml`:

```yaml
services:
  video-nginx:
    image: nginx:alpine
    container_name: video-demo-nginx
    restart: unless-stopped
    ports:
      - "5983:80"
    volumes:
      - /volume1/Docker/video-demo/html:/usr/share/nginx/html:ro
      - /volume1/Docker/video-demo/hls:/usr/share/nginx/hls:ro
      - /volume1/Docker/video-demo/nginx:/etc/nginx/conf.d:ro

  video-ffmpeg:
    image: linuxserver/ffmpeg:latest
    container_name: video-demo-ffmpeg
    restart: unless-stopped
    network_mode: host
    command:
      - -fflags
      - +genpts+discardcorrupt
      - -use_wallclock_as_timestamps
      - "1"
      - -rtsp_transport
      - udp
      - -i
      - rtsp://192.168.0.177:5983/stream
      - -an
      - -vf
      - fps=6
      - -c:v
      - libx264
      - -preset
      - ultrafast
      - -tune
      - zerolatency
      - -crf
      - "32"
      - -g
      - "12"
      - -keyint_min
      - "12"
      - -sc_threshold
      - "0"
      - -f
      - hls
      - -hls_time
      - "2"
      - -hls_list_size
      - "20"
      - -hls_flags
      - temp_file
      - -hls_segment_filename
      - /hls/stream_%03d.ts
      - /hls/index.m3u8
    volumes:
      - /volume1/Docker/video-demo/hls:/hls
```

В этой схеме `ffmpeg` не просто копирует входной H.264 bitstream, а перекодирует поток в `libx264`. Это сделано для более устойчивого HLS-вывода при нестабильном входном frame pacing. Параметры `-use_wallclock_as_timestamps 1`, `-fflags +genpts+discardcorrupt`, `fps=6`, короткий GOP и `zerolatency` уменьшают зависимость HLS-сегментации от неровного входного RTP/RTSP-потока.

## Сборка

Минимальные зависимости:

- Raspberry Pi 4 / Raspberry Pi OS;
- CSI camera;
- C++20 compiler;
- CMake 3.20+;
- FFmpeg development libraries:
  - `libavcodec`;
  - `libavutil`;
  - `libswscale`;
- POSIX/Linux networking API.

Пример установки зависимостей:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  pkg-config \
  libavcodec-dev \
  libavutil-dev \
  libswscale-dev \
  ffmpeg
```

Сборка:

```bash
git clone https://github.com/laib1e/video_streamer.git
cd video_streamer
mkdir -p build
cd build
cmake ..
cmake --build . -j$(nproc)
```

## Запуск

Пример текущего запуска из `main.cpp`:

```text
Camera: /dev/video14
Resolution: 640x480
FPS: 8
Bitrate: 1 Mbps
RTSP port: 5983
```

RTSP URL:

```text
rtsp://<raspberry-pi-ip>:5983/stream
```

Открыть в VLC:

```bash
vlc rtsp://<raspberry-pi-ip>:5983/stream
```

Или через GUI:

```text
Media → Open Network Stream → rtsp://<raspberry-pi-ip>:5983/stream
```

## Ограничения v1

Текущая версия — не production camera server.

Ограничения:

- capture пока через V4L2 compatibility layer, не native libcamera;
- один video track;
- RTP/UDP unicast;
- RTCP не реализован;
- RTP over RTSP TCP interleaved не реализован;
- authentication/TLS не реализованы;
- multi-client media fanout ограничен текущей реализацией streamer’а;
- запись архива в файл пока не реализована в основном pipeline;
- параметры stream пока частично заданы в `main.cpp`.

## План v2

Планируемая v2:

```text
native libcamera capture
    ↓
H.264 encoder
    ↓
encoded stream fanout
    ├── RTSP live clients
    └── file recorder / archive segments
```

Основные задачи v2:

- заменить V4L2 compatibility layer на native libcamera backend;
- вынести stream configuration в единую структуру;
- поддержать fanout encoded stream на несколько потребителей;
- добавить параллельную запись в файл независимо от RTSP clients;
- сделать multi-client RTSP/RTP delivery;
- улучшить resource ownership через RAII;
- добавить более чистый CMake layout;
- добавить scripts/systemd для запуска сервиса.

## License

MIT