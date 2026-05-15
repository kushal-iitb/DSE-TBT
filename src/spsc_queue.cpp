#include "spsc_queue.hpp"

namespace DSE::spsc{
    bool DSE::spsc::SpscQueue::open_consumer(const char* shm_name){
        fd = ::shm_open(shm_name , O_RDWR , 0644);
        if(fd == -1){
            return false;
        }
        base = ::mmap(nullptr , SHM_TOTAL_BYTES , PROT_READ | PROT_WRITE , MAP_SHARED , fd , 0);
        if(base == MAP_FAILED){
            ::close(fd);
            return false;
        }
        mapped_bytes = SHM_TOTAL_BYTES;
        ctrl = static_cast<ControlBlock*>(base);
        slots = reinterpret_cast<Slot*>(static_cast<char*>(base)+sizeof(ControlBlock));
        if(ctrl->magic != SHM_MAGIC){
            close();
            return false;
        }
        return true;
    }

    void DSE::spsc::SpscQueue::close(){
        if(base){
            ::munmap(base , mapped_bytes);
            base = nullptr;
            mapped_bytes = 0;
        }
        if(fd != -1){
            ::close(fd);
            fd = -1;
        }    
        ctrl = nullptr;
        slots = nullptr;
    }

    size_t DSE::spsc::SpscQueue::try_pop(void* out) noexcept {
        
        const uint64_t r = ctrl->read_pos.load(std::memory_order_relaxed);
        const uint64_t w = ctrl->write_pos.load(std::memory_order_acquire);

        if(r==w)
        return 0;


        std::memcpy(out , slots[r & QUEUE_MASK].data , SLOT_SIZE);
        ctrl->read_pos.store(r+1 , std::memory_order_release);
        
        return SLOT_SIZE;

    }


}