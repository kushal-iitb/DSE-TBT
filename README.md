# DSE-TBT — Tick-by-Tick Market Data Broadcaster

Reads every order-book event from the [DSE](../DSE) exchange's shared-memory queue and broadcasts it via UDP multicast. Maintains an in-memory order book reconstructed from the tick stream, plus an mmap'd tick log to serve recovery requests from any subscriber that fell behind.

## Architecture

```
                         ┌──────────────────────────────────────────────┐
DSE matching engine ───► │ SPSC SHM ring `/dev/shm/tbt_shm` (64 MB)     │
                         └──────────────────────┬───────────────────────┘
                                                ▼
            ┌───────────────────────────────────────────────────────────┐
            │ dse-tbt main loop (single thread)                         │
            │   while (try_pop(slot)) {                                 │
            │       publisher.send(slot)         ─► UDP 239.1.1.1:30001 │
            │       book.on_tick(slot)           ─► in-memory orderbook │
            │       history.record(seq, slot)    ─► mmap /tmp/...bin    │
            │   }                                                       │
            └─────────────────┬─────────────────────────────────────────┘
                              │
                              ▼
            ┌───────────────────────────────────────────────────────────┐
            │ TcpHandler (port 30002) — recovery server                 │
            │   subscriber sends 11-byte SnapshotRecoveryRequest:       │
            │     msg_type='S' → respond with FULL orderbook snapshot   │
            │     msg_type='R' → replay tick range [start_seq, end_seq] │
            │                    from mmap'd history file               │
            └───────────────────────────────────────────────────────────┘
```

## Layout

| File | Role |
|---|---|
| `src/main.cpp` | Pulls slots from SHM, fans out to multicast + orderbook + history. |
| `src/multicast_publisher.cpp` | UDP `IP_MULTICAST_TTL`/`IP_MULTICAST_LOOP` setup + `sendto`. |
| `src/orderbook.cpp` | Per-tick orderbook reconstruction (`unordered_map<orderId, OrderInfo>`), exposes `getSnapshot()` + `getLastSeqNo()`. |
| `src/tick_history.cpp` | mmap'd append log indexed by seq_no, 16M slots × 64 B = 1 GB file at `/tmp/dse_tbt_ticks.bin`. Survives restarts. |
| `src/spsc_queue.cpp` | Lock-free SHM ring (consumer side; vendored from DSE). |
| `src/TcpHandler.cpp` | TCP recovery server. Branches on `msg_type` between full snapshot and range replay. |
| `include/nse_fo_structs.hpp` | Wire structs: `OrderMessage`, `TradeMessage`, `SnapshotRecoveryRequest/Response`, `SnapshotHeader`, `SnapshotOrderRecord`. |

## Wire formats

**Multicast feed (UDP 239.1.1.1:30001)** — one 64-byte slot per packet, `OrderMessage` layout. `msg_type`: `'N'`/`'M'`/`'C'`/`'T'` (new/modify/cancel/trade). Sequence numbers monotonic per stream.

**Recovery TCP (port 30002)** — subscriber sends 11-byte `SnapshotRecoveryRequest` (msg_type + stream_id + start_seq + end_seq), server replies with a 10-byte `SnapshotRecoveryResponse` then either:
- `msg_type='S'` → 16-byte `SnapshotHeader` + N × 30-byte `SnapshotOrderRecord` (current book state).
- `msg_type='R'` → N × 64-byte slot (raw ticks for the requested seq range).

Server closes the connection after sending. Single-shot per connect.

## Build & run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Requires DSE to be running first (writes the SHM)
./build/dse-tbt
```

Manual recovery test:
```bash
# Tick range 2..3
printf 'R\x01\x00\x02\x00\x00\x00\x03\x00\x00\x00' | nc -q 2 localhost 30002 | xxd

# Full snapshot
printf 'S\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00' | nc -q 2 localhost 30002 | xxd
```

## Status

- ✅ SHM consumer + UDP multicast publishing
- ✅ In-memory orderbook reconstructed from N/M/C/T ticks (`mutex`-protected)
- ✅ mmap'd tick log (1 GB, indexed by seq, persists across restarts)
- ✅ Full-snapshot recovery (`msg_type='S'`)
- ✅ Tick-range recovery (`msg_type='R'`)
- 🚧 Modify staleness — producer sends new orderId only; old id lingers in book until clean disambiguation
- 🚧 Heartbeat on the multicast feed during idle periods
- 🚧 Snapshot-then-replay handshake for cold-start subscribers
- 🚧 Hugepage-backed SHM + tick log; CPU pinning
