#pragma once

#include"nse_fo_structs.hpp"
#include "logging_object.hpp"

#include<sys/mman.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<unistd.h>
#include<cerrno>
#include<cstring>
#include<cstdio>
#include<atomic>
#include<array>
#include<cstdint>

namespace DSE::spsc{

    constexpr size_t SLOT_SIZE = 64;
    constexpr size_t QUEUE_CAPACITY = 1u<<20;
    constexpr size_t QUEUE_MASK = QUEUE_CAPACITY-1;
    constexpr uint64_t SHM_MAGIC = 0x44534553'50534331ULL;

    struct alignas(64) Slot{
        uint8_t data[SLOT_SIZE];
    };
    static_assert(sizeof(Slot) == 64);

    struct ControlBlock{
        alignas(64) std::atomic<uint64_t> write_pos;
        alignas(64) std::atomic<uint64_t> read_pos;
        alignas(64) uint64_t magic;
        uint64_t capacity;
        uint64_t slot_size;
        uint64_t version;
        uint8_t pad[32];
    };
    static_assert(sizeof(ControlBlock) == 64*3);

    constexpr size_t SHM_TOTAL_BYTES = sizeof(ControlBlock) + QUEUE_CAPACITY*sizeof(Slot);


    class SpscQueue{
        public:

        SpscQueue() = default;
        ~SpscQueue(){
            close();
        }

        SpscQueue(const SpscQueue&) = delete;
        SpscQueue& operator=(const SpscQueue&) = delete;

        bool open_consumer(const char* shm_name);
        void close();

        size_t try_pop(void* out) noexcept;

        private:
        int fd = -1;
        void* base = nullptr;
        size_t mapped_bytes = 0;
        ControlBlock* ctrl = nullptr;
        Slot* slots = nullptr;
        char shm_name[64] = {};
    };
} // namespace DSE::spsc