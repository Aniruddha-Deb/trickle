#include <sys/socket.h>
#include <errno.h>

#include "server.hpp"

void Server::can_read_UDP() {
    std::shared_ptr<FileChunk> c(new FileChunk);
    int nb = recvfrom(_udp_ss, c.get(), sizeof(FileChunk), 0, nullptr, 0);

    if (nb == -1) {
        std::cerr << "Error while reading from UDP socket (errno " << errno << ")" << std::endl;
    }
    else {
        // handle the event where we don't read a complete filechunk in...
        // actually, such an event won't come up due to the low-water level
        // we've set.
        received_chunk(c);
    }
}