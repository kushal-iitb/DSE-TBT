#include "multicast_publisher.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace DSE::tbt {

bool MulticastPublisher::open(const char* group_ip, uint16_t port, uint8_t ttl) {
    sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ == -1) return false;

    if (::setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) == -1) {
        close();
        return false;
    }

    // 1 → packets are delivered to subscribers on the same host (useful for dev).
    // 0 → packets only leave the host on the wire.
    uint8_t loop = 1;
    if (::setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) == -1) {
        close();
        return false;
    }

    dst_.sin_family = AF_INET;
    dst_.sin_port   = htons(port);
    if (::inet_pton(AF_INET, group_ip, &dst_.sin_addr) != 1) {
        close();
        return false;
    }
    return true;
}

bool MulticastPublisher::send(const void* data, size_t bytes) {
    ssize_t n = ::sendto(sock_, data, bytes, MSG_DONTWAIT,
                         reinterpret_cast<sockaddr*>(&dst_), sizeof(dst_));
    return n == static_cast<ssize_t>(bytes);
}

void MulticastPublisher::close() {
    if (sock_ != -1) {
        ::close(sock_);
        sock_ = -1;
    }
}

} // namespace DSE::tbt
