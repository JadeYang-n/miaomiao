# Miaomiao

An AI-powered anti-fraud companion for elderly users, embodied as a talking pet cat on an ESP32 device with a screen, microphone, and speaker.

Miaomiao detects scam conversations in real-time, alerts family members via Feishu, and uses voice interaction to warn and protect elderly users.

## Features

- Real-time scam detection (health products, fake investments, impersonation, etc.)
- Voice conversation with AI cat personality
- ST7789 TFT display with animated cat expressions
- Automatic alerts to family members via Feishu
- User profile system (elderly person + family info)
- News-backed scam warnings with real case references
- Smart sleep mode (auto-sleep after inactivity)

## Hardware

| Component | Model |
|-----------|-------|
| MCU | ESP32-S3-DevKitC-1 (WROOM N16R8) |
| Microphone | INMP441 (I2S digital) |
| Speaker | MAX98357A (I2S amplifier) |
| Display | 1.54" ST7789 TFT (240x240 SPI) |

## Architecture

```
ESP32-S3 (miaomiao-firmware)
  ├── Audio capture (INMP441) → Wake word detection (WakeNet)
  ├── Voice activity detection → WebSocket → Backend
  ├── MP3 playback (MAX98357A)
  └── Cat animation display (ST7789)
        │
        ▼
Spring Boot Backend (miaomiao-backend)
  ├── ASR: speech-to-text
  ├── LLM: conversation + scam analysis
  ├── TTS: text-to-speech
  ├── Scam detection engine
  ├── News search (RSS + API)
  ├── Feishu bot integration
  └── H2 database (chat history + user profiles)
        │
        ▼
Vue 3 Frontend (miaomiao-frontend)
  └── Web interface for family members
```

## Quick Start

### Prerequisites

- Java 21+
- Node.js 18+
- ESP-IDF 5.4+
- An LLM API endpoint (OpenAI-compatible)
- A TTS API endpoint (OpenAI-compatible)
- Feishu app (optional, for family alerts)

### Backend

```bash
cd miaomiao-backend

# Copy and edit environment variables
cp ../.env.example ../.env
# Fill in your API keys

# Set environment variables
export LLM_API_KEY=your-key
export LLM_BASE_URL=https://your-llm-endpoint/v1
export LLM_MODEL=your-model-name
export TTS_API_KEY=your-key
export TTS_BASE_URL=https://your-tts-endpoint/v1/chat/completions
export TTS_MODEL=your-tts-model
export FEISHU_APP_ID=your-app-id
export FEISHU_APP_SECRET=your-app-secret
export FEISHU_USER_OPEN_ID=your-open-id

# Run
./mvnw spring-boot:run
```

The backend starts on port 8080.

### Firmware

```bash
cd miaomiao-firmware

# Configure WiFi and backend address
# Edit main/config.h with your settings

# Build and flash
idf.py build flash monitor
```

### Frontend

```bash
cd miaomiao-frontend
npm install
npm run dev
```

## Project Structure

```
miaomiao/
├── miaomiao-backend/       # Spring Boot backend (Java 21)
│   ├── src/main/java/com/antifraud/
│   │   ├── config/         # WebClient, LLM, TTS config
│   │   ├── service/        # AI, TTS, notification services
│   │   ├── websocket/      # ESP32 WebSocket handler
│   │   ├── controller/     # REST API endpoints
│   │   └── entity/         # Database entities
│   └── src/main/resources/
│       └── application.yml
├── miaomiao-firmware/      # ESP32-S3 firmware (C, ESP-IDF)
│   └── main/
│       ├── audio_service.c # Audio pipeline + state machine
│       ├── display.c       # ST7789 cat animation
│       └── config.h        # Hardware pin configuration
├── miaomiao-frontend/      # Vue 3 web frontend
├── mqtt-udp-gateway/       # MQTT/UDP bridge (optional)
└── .env.example            # Environment variable template
```

## Risk Levels

| Level | Description | Action |
|-------|-------------|--------|
| 0 | Normal conversation | No action |
| 1 | Medium risk | Gentle reminder |
| 2 | High risk | Strong warning + Feishu alert to family |

## License

[MIT](LICENSE)

## Acknowledgments

- [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) — ESP32 firmware architecture and audio pipeline reference
