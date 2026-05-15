#include "logging_object.hpp"
#include "spsc_queue.hpp"
#include "multicast_publisher.hpp"

int main(){
    Logger::init("DSE_TBT" , "logs" , 2);

    const char* shm_name = "tbt_shm";

    DSE::spsc::SpscQueue tbt_queue;
    DSE::tbt::MulticastPublisher publisher;
    publisher.open("239.1.1.1" , 30001);
    if(!tbt_queue.open_consumer(shm_name)){
        DSE_LOG_ERROR(" open consumer failed ");
        return 1;
    }
    DSE_LOG_INFO(" dse_tbt consumer ready on shm = {} ", shm_name);

    uint8_t slot[DSE::spsc::SLOT_SIZE];

    uint64_t got = 0;
    while(true){
        if(tbt_queue.try_pop(slot) ==0 ){
            continue;
        }
        auto* m = reinterpret_cast<DSE::fo::OrderMessage*>(slot);
        publisher.send(m , sizeof(*m));
         DSE_LOG_INFO("recv seq={} type={} orderId={} token={} price={} qty={}",
                     m->header.seq_no, m->msg_type, (int64_t)m->order_id,
                     m->token, m->price, m->quantity);
    }

}