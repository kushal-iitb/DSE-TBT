#pragma once

#include <cstddef>
#include <cstdint>
#include <netinet/in.h>

namespace DSE::tbt {

class MulticastPublisher {
public:
    MulticastPublisher() = default;
    ~MulticastPublisher() { close(); }

    MulticastPublisher(const MulticastPublisher&)            = delete;
    MulticastPublisher& operator=(const MulticastPublisher&) = delete;

    // group_ip: multicast group, e.g. "239.1.1.1" (administratively scoped /8 range).
    // port:     UDP port to publish on.
    // ttl:      1 = same host only, 4 = same site, 32+ = WAN. Keep low for dev.
    bool open(const char* group_ip, uint16_t port, uint8_t ttl = 4);

    // Returns true on full send. Non-blocking (MSG_DONTWAIT).
    bool send(const void* data, size_t bytes);

    void close();

private:
    int         sock_ = -1;
    sockaddr_in dst_{};
};

} // namespace DSE::tbt
