#pragma once

#include<atomic>
#include<mutex>
#include<unordered_map>
#include<map>
#include<vector>

#include "logging_object.hpp"
#include "spsc_queue.hpp"



namespace DSE::orderbook{
    
    struct OrderInfo{
    uint32_t orderId;
    uint32_t token;
    char orderType;
    int32_t price;
    uint32_t qty;
    };


    class orderbook{
        private:
        std::atomic<uint32_t> seq_no{0};
        std::unordered_map<uint32_t , OrderInfo> orders;
        mutable std::mutex mt_;

        public:
        std::vector<OrderInfo> getSnapshot();
        uint32_t getLastSeqNo();
        void on_tick(DSE::fo::OrderMessage& m);
        void onNew(DSE::fo::OrderMessage& m);
        void onModify(DSE::fo::OrderMessage& m);
        void onCancel(DSE::fo::OrderMessage& m);
        void onTrade(DSE::fo::OrderMessage& m);
    };
}