#include "logging_object.hpp"
#include "spsc_queue.hpp"
#include "multicast_publisher.hpp"
#include "TcpHandler.hpp"
#include "orderbook.hpp"
#include "tick_history.hpp"

int main(){
    Logger::init("DSE_TBT" , "logs" , 2);

    const char* shm_name = "tbt_shm";
    const char* history_path = "/tmp/dse_tbt_ticks.bin";

    DSE::spsc::SpscQueue tbt_queue;
    DSE::tbt::MulticastPublisher publisher;
    DSE::orderbook::orderbook book;
    DSE::tbt::TickHistory history;
    if(!history.open(history_path)){
        DSE_LOG_ERROR(" tick history open failed ");
        return 1;
    }
    DSE::TcpHandler::TcpHandler recovery_server("30002", &book, &history);
    publisher.open("239.1.1.1" , 30001);
    if(!tbt_queue.open_consumer(shm_name)){
        DSE_LOG_ERROR(" open consumer failed ");
        return 1;
    }
    if(!recovery_server.setup()){
        DSE_LOG_ERROR(" recovery server setup failed ");
        return 1;
    }
    recovery_server.start();
    DSE_LOG_INFO(" recovery server listening on 30002 ");
    DSE_LOG_INFO(" tick history mmap'd at {} (latest seq = {}) ", history_path, history.latest());
    DSE_LOG_INFO(" dse_tbt consumer ready on shm = {} ", shm_name);

    uint8_t slot[DSE::spsc::SLOT_SIZE];

    while(true){
        if(tbt_queue.try_pop(slot) ==0 ){
            continue;
        }
        auto* m = reinterpret_cast<DSE::fo::OrderMessage*>(slot);
        publisher.send(m , sizeof(*m));
        book.on_tick(*m);
        history.record(m->header.seq_no, slot, DSE::spsc::SLOT_SIZE);
         DSE_LOG_INFO("recv seq={} type={} orderId={} token={} price={} qty={}",
                     m->header.seq_no, m->msg_type, (int64_t)m->order_id,
                     m->token, m->price, m->quantity);
    }

}