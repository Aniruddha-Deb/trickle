// this uses a TCP control layer over a UDP data layer

#include <sys/socket.h>
#include <errno.h>
#include "client.hpp"

void Client::configure_tcp(uintptr_t tcp_fd) {
    // set socket low-water mark value for input to be the size of one control message
    const int sock_recv_lwm = sizeof(ControlMessage);
    setsockopt(_tcp_sock, SOL_SOCKET, SO_RCVLOWAT, &sock_recv_lwm, sizeof(sock_recv_lwm));
}

void Client::can_read_TCP() {
    ControlMessage m;
    ssize_t nb = recv(_tcp_sock, &m, sizeof(m), 0);

    if (nb == -1) {
        std::cerr << "Error while reading from TCP socket (errno " << errno << ")" << std::endl;
    }
    else if (nb == 0) {
        // this happens when the socket has disconnected
        // have a callback that detatches this socket from everything
        disconnected();
    }
    else {
        // handle the event where we don't read a complete filechunk in...
        // actually, such an event won't come up due to the low-water level
        // we've set.
        received_control(m);
    }   
}

void Client::can_read_UDP() {
    // ControlMessage req_data;
    std::unique_ptr<FileChunk> c = std::make_unique<FileChunk>();
    ssize_t nb = recvfrom(_udp_sock, c.get(), sizeof(FileChunk), 0, nullptr, 0);

    if (nb == -1) {
        std::cerr << "Error while reading from UDP socket (errno " << errno << ")" << std::endl;
    }
    else {
        received_chunk(std::move(c));
    }
}

void Client::can_write_TCP() {
    if (!_control_msg_buffer.empty()) {
        ControlMessage c = _control_msg_buffer.front(); // not worrying about network byte order for now.
        ssize_t nb = send(_tcp_sock, &c, sizeof(c), 0);
        if (nb == -1) {
            std::cerr << "Error while writing to TCP socket (errno " << errno << ")" << std::endl;
        }
        else {
            _control_msg_buffer.pop();
        }
    }
}

void Client::can_write_UDP() {
    if (!_chunk_buffer.empty()) {
        uint32_t p = _chunk_buffer.front();
        ssize_t nb = sendto(_udp_sock, _chunks[p].get(), sizeof(FileChunk), 0, nullptr, 0);

        if (nb == -1) {
            std::cerr << "Error while writing to UDP socket (err " << errno << ")" << std::endl;
        }
        else {
            _chunk_buffer.pop();
            sent_chunk(p);
        }
    }
}
