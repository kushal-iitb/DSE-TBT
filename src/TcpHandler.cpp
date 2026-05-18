#include "TcpHandler.hpp"
#include "logging_object.hpp"
#include "nse_fo_structs.hpp"
#include "orderbook.hpp"

namespace DSE :: TcpHandler{

    TcpHandler::TcpHandler(char* port , DSE::orderbook::orderbook* ob , DSE::tbt::TickHistory* hist){
        this->PORT = port;
        this->ob = ob;
        this->hist = hist;
    }

    bool TcpHandler ::setup() {
    memset(&hints , 0 , sizeof(hints)); 
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    if((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) !=0){
        DSE_LOG_ERROR("error in getaddrinfo function");
        return false;
    }

    for(p = servinfo; p!=NULL; p = p->ai_next){
        if((sockfd = socket(p->ai_family , p->ai_socktype , p->ai_protocol)) == -1){
                DSE_LOG_ERROR("error in socket function");
                freeaddrinfo(servinfo);
                return false;
        }

        if(setsockopt(sockfd , SOL_SOCKET, SO_REUSEADDR, &optval , sizeof(int)) == -1){
            DSE_LOG_ERROR("error in setsockopt function");
            freeaddrinfo(servinfo);
            return false;
        }

        if(bind(sockfd, p->ai_addr , p->ai_addrlen) == -1){
            DSE_LOG_ERROR("error in bind function");
            close(sockfd);
            sockfd = -1;
            freeaddrinfo(servinfo);
            return false;
        }

        if(listen(sockfd, SOMAXCONN) == -1){
            DSE_LOG_ERROR("error in listen function");
            close(sockfd);
            sockfd = -1;
            freeaddrinfo(servinfo);
            return false;
        }
        break;
    }

    freeaddrinfo(servinfo);

    if( p == NULL){
        DSE_LOG_INFO("server failed to bind");
        return false;
    }

    return true;
}

    int TcpHandler::accept_connections(){
        
        sockaddr_storage client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int connection = accept(sockfd, (sockaddr*)&client_addr , &addr_size);
        if(connection == -1){
            DSE_LOG_ERROR("error in accept function , errno = {}" , errno);
            return -1;
        }
        return connection;
    }
    

    void TcpHandler::start(){
        tcp_connection_thread  = std::thread([this]{
            while(true){
            int conn = accept_connections();
            if(conn!=-1){
            DSE_LOG_INFO(" connection accepted");
            std::lock_guard<std::mutex> lock(mt);
            connection_fds.push_back(conn);
            }
        }
    });

        tcp_recv_thread  = std::thread([this](){
        while(true){
            recvdata();
        }
    });

        tcp_connection_thread.detach();
        tcp_recv_thread.detach();
    }

            


    void TcpHandler::recvdata(){
        std::lock_guard<std::mutex> lock(mt);
        if(connection_fds.empty())
        return;
        std::vector<int> done_fds;
        for(auto it : connection_fds){
            char snapshot_request[11] = {};
            int data = recv(it, snapshot_request , sizeof(snapshot_request) , MSG_DONTWAIT);
            if(data == sizeof(snapshot_request)){
                auto* req = reinterpret_cast<DSE::fo::SnapshotRecoveryRequest*>(snapshot_request);

                if(req->msg_type == 'R'){
                    // ----- Tick recovery: replay seqs [start_seq_no, end_seq_no] -----
                    DSE::fo::SnapshotRecoveryResponse resp{};
                    resp.header.msg_len   = sizeof(resp);
                    resp.header.stream_id = req->stream_id;
                    resp.header.seq_no    = 0;
                    resp.msg_type         = 'Y';
                    resp.request_status   = (req->start_seq_no > 0 && req->start_seq_no <= req->end_seq_no) ? 'S' : 'E';
                    send(it, reinterpret_cast<char*>(&resp), sizeof(resp));

                    if(resp.request_status == 'S' && hist){
                        const uint32_t latest = hist->latest();
                        const uint32_t to     = std::min(req->end_seq_no, latest);
                        uint8_t buf[DSE::tbt::TickHistory::SLOT_SIZE];
                        for(uint32_t s = req->start_seq_no; s <= to; ++s){
                            if(hist->try_fetch(s, buf)){
                                send(it, reinterpret_cast<char*>(buf), sizeof(buf));
                            }
                        }
                    }
                    ::close(it);
                    done_fds.push_back(it);
                }
                else {
                    // ----- Snapshot recovery (existing path) -----
                    DSE::fo::SnapshotRecoveryResponse snapshot_response{};
                    snapshot_response.header.msg_len   = sizeof(snapshot_response);
                    snapshot_response.header.stream_id = 1;
                    snapshot_response.header.seq_no    = 0;
                    snapshot_response.msg_type         = 'B';
                    snapshot_response.request_status   = 'S';
                    char buffer[sizeof(snapshot_response)];
                    memcpy(buffer , &snapshot_response, sizeof(snapshot_response));
                    send(it , buffer , sizeof(snapshot_response));


                    DSE::fo::SnapshotHeader snapshot_header;
                    std::vector<DSE::orderbook::OrderInfo> orders = ob->getSnapshot();
                    uint32_t seq_no = ob->getLastSeqNo();

                    snapshot_header.last_sequence_no = seq_no;
                    snapshot_header.num_records = orders.size();
                    snapshot_header.size = sizeof(snapshot_header)+orders.size()*sizeof(DSE::fo::SnapshotOrderRecord);
                    snapshot_header.stream_id = 1;
                    snapshot_header.trans_code = 10501;

                    std::vector<char> snapshot_buffer(snapshot_header.size, 0);
                    memcpy(snapshot_buffer.data(), &snapshot_header, sizeof(snapshot_header));

                    size_t order_index = sizeof(snapshot_header);
                    for(auto it: orders){
                        DSE::fo::SnapshotOrderRecord snapshot_order{};
                        snapshot_order.msg_type = 'N';
                        snapshot_order.order_id = it.orderId;
                        snapshot_order.token = it.token;
                        snapshot_order.order_type = it.orderType;
                        snapshot_order.price = it.price;
                        snapshot_order.quantity = it.qty;
                        snapshot_order.timestamp_ns = 1;
                        memcpy(snapshot_buffer.data() + order_index, &snapshot_order, sizeof(snapshot_order));
                        order_index += sizeof(snapshot_order);
                    }

                    send(it, snapshot_buffer.data(), snapshot_buffer.size());
                    ::close(it);
                    done_fds.push_back(it);
                }
            }
            else if(data == 0){
                // peer closed before sending full request
                ::close(it);
                done_fds.push_back(it);
            }

        }

        for(int fd : done_fds){
            connection_fds.erase(
                std::remove(connection_fds.begin(), connection_fds.end(), fd),
                connection_fds.end());
        }
    }

    void TcpHandler::stop(){

    }

    void TcpHandler::send(int fd , char* buffer , size_t len){

        ::send(fd, buffer, len , 0);

    }

 
}