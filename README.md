# video_streamer

Захват видео с CSI-камеры Raspberry Pi 4 и передача по сети через RTP/UDP.

## Архитектура

```
[CameraSource] → LockFreeQueue<Frame, 64> → [VideoStreamer<RtpUdpTransport>] → UDP
```

Проект построен на двух потоках, связанных lock-free очередью (SPSC ring buffer):

- **LockFreeQueue** — lock-free SPSC очередь фиксированного размера с поддержкой move-семантики. Обеспечивает передачу кадров между потоками без мьютексов.

## Зависимости

- Raspberry Pi 4 с Raspberry Pi OS (Bullseye или новее)
- CSI Camera Module (v2 или v3)
- Компилятор с поддержкой C++20 (GCC 12+)
- CMake 3.20+

## Сборка

```bash
git clone <repo-url>
cd video_streamer_project
mkdir build && cd build
cmake ..
make -j4
```

## RTP

Используется упрощённый RTP-заголовок (12 байт, RFC 3550):

- Version: 2
- Payload type: 96 (dynamic)
- Timestamp clock: 90 kHz
- Каждый кадр отправляется одним UDP-пакетом

## Лицензия

MIT