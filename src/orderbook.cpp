#include "orderbook.hpp"


void DSE::orderbook::orderbook::on_tick(DSE::fo::OrderMessage& m){
    std::lock_guard<std::mutex> lk(mt_);
    switch(m.msg_type){
        case 'N': onNew(m);    break;
        case 'M': onModify(m); break;
        case 'C': onCancel(m); break;
        case 'T': onTrade(m);  break;
        default: break;
    }
    seq_no.store(m.header.seq_no, std::memory_order_release);
}

void DSE::orderbook::orderbook::onNew(DSE::fo::OrderMessage& m){
        // caller (on_tick) holds mt_
        OrderInfo orderinfo;
        orderinfo.orderId = (uint32_t)m.order_id;
        orderinfo.token = (uint32_t)m.token;
        orderinfo.orderType = m.order_type;
        orderinfo.price  = m.price;
        orderinfo.qty = m.quantity;

        orders[orderinfo.orderId] = orderinfo;

        return;

}

void DSE::orderbook::orderbook::onModify(DSE::fo::OrderMessage& m){
        // caller (on_tick) holds mt_
        onNew(m);

}

void DSE::orderbook::orderbook::onCancel(DSE::fo::OrderMessage& m){
        // caller (on_tick) holds mt_
    orders.erase(m.order_id);

}

void DSE::orderbook::orderbook::onTrade(DSE::fo::OrderMessage& m){
        // caller (on_tick) holds mt_
    auto it = orders.find(m.order_id);

    if(it == orders.end())
    return;

    if((uint32_t)m.quantity >= it->second.qty){
        orders.erase(it);
    }
    else{
        it->second.qty -= m.quantity;
    }

    return;
}

uint32_t DSE::orderbook::orderbook::getLastSeqNo(){
    return seq_no.load(std::memory_order_acquire);
}

    std::vector<DSE::orderbook::OrderInfo> DSE::orderbook::orderbook::getSnapshot(){
    std::lock_guard<std::mutex> lk(mt_);
    std::vector<OrderInfo> snapshot;
    snapshot.reserve(orders.size());

    for(auto &it: orders){
        snapshot.push_back(it.second);
    }
    return snapshot;
}