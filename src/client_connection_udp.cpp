#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <queue>
#include <iostream>

#include "client_connection.hpp"
#include "protocol.hpp"

// this implementation uses a UDP control layer over a TCP data layer

ClientConnection::ClientConnection(
    uint32_t client_id, uintptr_t tcp_fd, uintptr_t udp_fd, struct sockaddr_in client_addr):
    _client_id{client_id},
    _tcp_fd{tcp_fd},
    _udp_fd{udp_fd} {

    // set tcp_fd to be non blocking
    fcntl(_tcp_fd, F_SETFL, O_NONBLOCK);

    // set socket low-water mark value for input to be the size of one file chunk
    const int sock_recv_lwm = sizeof(FileChunk);
    setsockopt(_tcp_fd, SOL_SOCKET, SO_RCVLOWAT, &sock_recv_lwm, sizeof(sock_recv_lwm));

    // we don't have to do something similar for send, as that would slow down 
    // the speed at which we're sending. Just having this at one end is okay

    // socket buffer sizes are generally big enough (on my mac, it's 128 kB) for
    // buffer overflows to be a non-issue, so we don't tweak the default buffer
    // size

    // The sequentiality of TCP guarantees that we're always reading a contiguous
    // chunk sent by client/server. So we don't need a protocol indicating length 
    // or anything of that sort; just chunk into 1024 bytes while reading.

    // timeouts are also ok; let's not change those.
    
    _client_addr = client_addr;
}

void ClientConnection::can_write_TCP() {
    if (!_chunk_buffer.empty()) {
        auto p = _chunk_buffer.front();
        ssize_t nb = send(_tcp_fd, p.get(), sizeof(FileChunk), 0);
        if (nb == -1) {
            std::cerr << "Error while writing to TCP socket " << get_addr_str() << " (errno " << errno << ")" << std::endl;
        }
        else {
            _chunk_buffer.pop();
            _send_chunk_callback(_client_id, p->id);
        }
    }
}

void ClientConnection::can_read_TCP() {
    std::shared_ptr<FileChunk> c(new FileChunk);
    ssize_t nb = recv(_tcp_fd, c.get(), sizeof(FileChunk), 0);

    if (nb == -1) {
        std::cerr << "Error while reading from TCP socket " << get_addr_str() << " (errno " << errno << ")" << std::endl;
    }
    else if (nb == 0) {
        // this happens when the socket has disconnected
        // have a callback that detatches this socket from everything
        close_client();
    }
    else {
        // handle the event where we don't read a complete filechunk in...
        // actually, such an event won't come up due to the low-water level
        // we've set.
        _recv_chunk_callback(c);
    }
}

void ClientConnection::can_write_UDP() {
    if (!_control_msg_buffer.empty()) {
        auto p = _control_msg_buffer.front(); // not worrying about network byte order for now.
        socklen_t len = sizeof(_client_addr);
        int nb = sendto(_udp_fd, &p, sizeof(p), 0, 
                        (struct sockaddr*)&_client_addr, len);

        if (nb == -1) {
            std::cerr << "Error while writing to UDP socket (err " << errno << ")" << std::endl;
        }
        else {
            // datagram oriented protocol, so no worries here
            //std::cout << "Wrote " << data << " (" << nb << " bytes) to UDP socket " << ntohs(p.first.sin_port) << std::endl;
            _control_msg_buffer.pop();
        }
    }
}
