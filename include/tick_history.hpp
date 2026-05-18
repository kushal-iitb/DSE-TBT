#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace DSE::tbt {

// On-disk tick log, mmapped into the process address space.
// Layout: [Header (64 bytes)] [Slot[0] (64)] [Slot[1] (64)] ...
// Slot index = seq_no - 1 (seqs are 1-based, matching the producer).
//
// Producer (main loop) writes via record(); consumer/recovery thread reads via try_fetch().
// latest_seq is published with release-store after the slot bytes are written; readers
// acquire-load it before reading the slot bytes.
class TickHistory {
public:
    static constexpr size_t   SLOT_SIZE = 64;
    static constexpr size_t   CAPACITY  = 1u << 24;     // 16M slots → 1 GB file
    static constexpr uint64_t MAGIC     = 0x44534554'42485431ULL;  // "DSET" "BHT1"

    struct Header {
        alignas(64) std::atomic<uint32_t> latest_seq;
        uint64_t magic;
        uint64_t version;
        uint32_t slot_size;
        uint64_t capacity;
        uint8_t  pad[32];
    };
    static_assert(sizeof(Header) >= 64);

    TickHistory() = default;
    ~TickHistory() { close(); }
    TickHistory(const TickHistory&)            = delete;
    TickHistory& operator=(const TickHistory&) = delete;

    bool open(const char* path);
    void close();

    // Producer: write `bytes` into slot[seq-1], then publish latest_seq.
    // No-op if seq is out of range.
    void record(uint32_t seq, const void* data, size_t bytes) noexcept;

    // Reader: copy SLOT_SIZE bytes into `out` if seq <= latest_seq && seq > 0 && seq <= CAPACITY.
    // Returns true on success.
    bool try_fetch(uint32_t seq, void* out_slot) const noexcept;

    uint32_t latest() const noexcept;

private:
    int      fd_           = -1;
    void*    base_         = nullptr;
    size_t   mapped_bytes_ = 0;
    Header*  hdr_          = nullptr;
    uint8_t* slots_        = nullptr;
};

} // namespace DSE::tbt
