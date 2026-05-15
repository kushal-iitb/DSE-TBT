#pragma once

#include<cstdint>

namespace DSE::fo{

#pragma pack(push , 1)

struct TBT_Header{
    uint16_t msg_len;
    uint16_t stream_id;
    uint32_t seq_no;
};
static_assert(sizeof(TBT_Header) == 8);


struct OrderMessage {
	TBT_Header header;
	char msg_type;
	std::int64_t timestamp_ns;
	double order_id;
	std::int32_t token;
	char order_type;
	std::int32_t price;
	std::int32_t quantity;
};

struct TradeMessage {
	TBT_Header header;
	char msg_type;
	std::int64_t timestamp_ns;
	double buy_order_id;
	double sell_order_id;
	std::int32_t token;
	std::int32_t trade_price;
	std::int32_t trade_quantity;
};

#pragma pack(pop)


}