#include "tick_history.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace DSE::tbt {

bool TickHistory::open(const char* path) {
    const size_t total = sizeof(Header) + CAPACITY * SLOT_SIZE;

    fd_ = ::open(path, O_CREAT | O_RDWR, 0644);
    if (fd_ == -1) {
        std::perror("tick_history open");
        return false;
    }

    if (::ftruncate(fd_, total) == -1) {
        std::perror("tick_history ftruncate");
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    base_ = ::mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (base_ == MAP_FAILED) {
        std::perror("tick_history mmap");
        ::close(fd_);
        fd_ = -1;
        base_ = nullptr;
        return false;
    }
    mapped_bytes_ = total;
    hdr_   = static_cast<Header*>(base_);
    slots_ = static_cast<uint8_t*>(base_) + sizeof(Header);

    // Initialize on a fresh file (magic mismatch). Existing files keep their state across restarts.
    if (hdr_->magic != MAGIC) {
        hdr_->capacity  = CAPACITY;
        hdr_->slot_size = SLOT_SIZE;
        hdr_->version   = 1;
        hdr_->latest_seq.store(0, std::memory_order_relaxed);
        hdr_->magic     = MAGIC;  // last → readers see init complete via magic check
    }
    return true;
}

void TickHistory::close() {
    if (base_) {
        ::munmap(base_, mapped_bytes_);
        base_         = nullptr;
        mapped_bytes_ = 0;
    }
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
    hdr_   = nullptr;
    slots_ = nullptr;
}

void TickHistory::record(uint32_t seq, const void* data, size_t bytes) noexcept {
    if (seq == 0 || seq > CAPACITY) return;
    if (bytes == 0 || bytes > SLOT_SIZE) return;
    if (!slots_) return;

    uint8_t* dst = slots_ + static_cast<size_t>(seq - 1) * SLOT_SIZE;
    std::memcpy(dst, data, bytes);
    if (bytes < SLOT_SIZE) {
        std::memset(dst + bytes, 0, SLOT_SIZE - bytes);
    }
    // Publish: any future load-acquire on latest_seq that returns >= seq is guaranteed
    // to see the bytes above.
    uint32_t current = hdr_->latest_seq.load(std::memory_order_relaxed);
    if (seq > current) {
        hdr_->latest_seq.store(seq, std::memory_order_release);
    }
}

bool TickHistory::try_fetch(uint32_t seq, void* out_slot) const noexcept {
    if (!slots_) return false;
    if (seq == 0 || seq > CAPACITY) return false;
    uint32_t latest = hdr_->latest_seq.load(std::memory_order_acquire);
    if (seq > latest) return false;
    const uint8_t* src = slots_ + static_cast<size_t>(seq - 1) * SLOT_SIZE;
    std::memcpy(out_slot, src, SLOT_SIZE);
    return true;
}

uint32_t TickHistory::latest() const noexcept {
    if (!hdr_) return 0;
    return hdr_->latest_seq.load(std::memory_order_acquire);
}

} // namespace DSE::tbt
