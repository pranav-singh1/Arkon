# Arkon

### Real-Time Prediction Market Intelligence Engine

Arkon is a low-latency market data aggregation and arbitrage detection system for prediction markets (Kalshi, Polymarket, Manifold). It consists of a high-performance C++ core engine, a Python ML signal layer, and a React dashboard.

---

## Project Goals

1. **Detect arbitrage in real time** — same underlying event priced differently across exchanges. Flag when the spread exceeds transaction cost thresholds.
2. **Generate mispricing signals** — train ML models on historical resolution data to identify when a market is _wrong_, not just when there's a cross-exchange spread.
3. **Execute or paper-trade signals** — Kelly criterion sizing, P&L tracking, signal accuracy feedback loop.
4. **Build interview-grade systems work** — lock-free concurrency, IPC, latency profiling, deployed ML. This is the technical foundation for a founder/FAANG narrative.

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────┐
│                      C++ Core Engine                     │
│                                                          │
│  [Kalshi WS] ──┐                                         │
│                ├──> [Lock-Free Ring Buffer] ──> [Arb     │
│  [Polymarket]──┘         (SPSC Queue)          Detector] │
│                                                    │     │
│                              [Flatfile / Shared Mem]     │
└──────────────────────────────────────────────────────────┘
                                    │
                    ┌───────────────▼──────────────┐
                    │       Python ML Layer        │
                    │  XGBoost / LightGBM models   │
                    │  Kelly criterion sizing      │
                    │  News sentiment features     │
                    └───────────────┬──────────────┘
                                    │
                    ┌───────────────▼──────────────┐
                    │       React Dashboard        │
                    │  Live contract browser       │
                    │  Arb alert feed              │
                    │  P&L tracker                 │
                    └──────────────────────────────┘
```

---

## Component Breakdown

### 1. C++ Core Engine (`src/`)

The performance-critical layer. Designed for minimal latency on the hot path.

**Key design decisions:**

- One thread per exchange WebSocket connection (producer threads)
- Lock-free SPSC (Single Producer Single Consumer) ring buffers between producers and the central aggregator — no mutex on the hot path
- Thread pool for pricing computations: implied probability normalization, vig extraction, spread detection
- Flatbuffers serialization for zero-copy data passing
- Output written to shared memory segment or binary flatfile for Python consumption

**Files:**

- `src/kalshi_feed.cpp` — Kalshi WebSocket client (Phase 1 starting point)
- `src/polymarket_feed.cpp` — Polymarket WebSocket client (Phase 2)
- `src/aggregator.cpp` — Central aggregator, arb detection logic
- `src/ring_buffer.hpp` — Lock-free SPSC queue implementation
- `src/main.cpp` — Entry point, thread orchestration

**Libraries:**

- `Boost.Asio` — async I/O and networking
- `Boost.Beast` — WebSocket on top of Asio
- `OpenSSL` — TLS for exchange connections
- `nlohmann/json` — JSON parsing
- `flatbuffers` — fast serialization (Phase 2+)

### 2. Python ML Layer (`ml/`)

Consumes C++ engine output via shared memory or Unix socket.

**Models:**

- XGBoost / LightGBM trained on historical contract resolution data
- Predicts probability of YES resolution — compare against market implied probability to find edge

**Features:**

- Volume imbalance (bid/ask size asymmetry)
- Time-to-resolution
- Correlated contract movements across exchanges
- News sentiment (free news API integration)
- Cross-market spread history

**Output:**

- Confidence-weighted signal per contract
- Kelly criterion position sizing recommendation

**Files:**

- `ml/consumer.py` — reads C++ output, feeds model
- `ml/train.py` — model training pipeline
- `ml/features.py` — feature engineering
- `ml/backtest.py` — historical signal evaluation

### 3. React Dashboard (`dashboard/`)

Real-time monitoring UI over WebSocket.

**Views:**

- Live contract browser: implied probability vs model estimate
- Arb alert feed with EV calculations
- P&L tracker (paper trade or live)
- Historical signal accuracy charts

**Stack:** React + Recharts + WebSocket

---

## Build & Run

### Prerequisites

```bash
# macOS
brew install cmake boost openssl nlohmann-json

# Ubuntu/Debian
sudo apt install cmake libboost-all-dev libssl-dev nlohmann-json3-dev
```

### Build C++ Engine

```bash
cmake -B build
cmake --build build
```

### Run Kalshi Feed

```bash
export KALSHI_API_KEY=your_key_here
./build/kalshi_feed
```

---

## Environment Variables

| Variable             | Description                        |
| -------------------- | ---------------------------------- |
| `KALSHI_API_KEY`     | Kalshi API key (never commit this) |
| `POLYMARKET_API_KEY` | Polymarket API key (Phase 2)       |

**Never hardcode keys. Never commit `.env` files.**

---

## Build Phases

| Phase | Goal                                                                   | Status         |
| ----- | ---------------------------------------------------------------------- | -------------- |
| 1     | Kalshi WS client — print live data to terminal                         | 🔧 In progress |
| 2     | Add threading — producer/consumer with lock-free queue, add Polymarket | ⬜ Planned     |
| 3     | Arb detection logic + Python consumer                                  | ⬜ Planned     |
| 4     | ML model on historical data, backtest framework                        | ⬜ Planned     |
| 5     | Dashboard, paper trading, signal iteration                             | ⬜ Planned     |

---

## Project Structure

```
arkon/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── src/                  # C++ core engine
│   ├── kalshi_feed.cpp
│   ├── polymarket_feed.cpp
│   ├── aggregator.cpp
│   ├── ring_buffer.hpp
│   └── main.cpp
├── ml/                   # Python ML layer
│   ├── consumer.py
│   ├── train.py
│   ├── features.py
│   └── backtest.py
└── dashboard/            # React frontend
    ├── src/
    └── package.json
```

---
