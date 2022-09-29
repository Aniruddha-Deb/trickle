// this implementation uses a TCP control layer over a UDP data layer

#include <fcntl.h>
#include <errno.h>
#include <iostream>

#include "client_connection.hpp"
#include "server.hpp"

ClientConnection::ClientConnection(
    uint32_t client_id, uintptr_t tcp_fd, uintptr_t udp_fd, struct sockaddr_in client_addr):
    _client_id{client_id},
    _tcp_fd{tcp_fd},
    _udp_fd{udp_fd} {

    // set tcp_fd to be non blocking
    fcntl(_tcp_fd, F_SETFL, O_NONBLOCK);

    // set socket low-water mark value for input to be the size of one control message
    const int sock_recv_lwm = sizeof(ControlMessage);
    setsockopt(_tcp_fd, SOL_SOCKET, SO_RCVLOWAT, &sock_recv_lwm, sizeof(sock_recv_lwm));
    
    _client_addr = client_addr;
}

void ClientConnection::can_write_TCP() {
    if (!_control_msg_buffer.empty()) {
        auto p = _control_msg_buffer.front();
        ssize_t nb = send(_tcp_fd, &p, sizeof(p), 0);
        if (nb == -1) {
            std::cerr << "Error while writing to TCP socket " << get_addr_str() << " (errno " << errno << ")" << std::endl;
        }
        else {
            _control_msg_buffer.pop();
        }
    }
}

void ClientConnection::can_read_TCP() {
    ControlMessage req_data;
    int nb = recv(_tcp_fd, &req_data, sizeof(req_data), 0);

    if (nb == -1) {
        std::cerr << "Error while reading from TCP socket " << get_addr_str() << " (errno " << errno << ")" << std::endl;
    }
    else if (nb == 0) {
        // this happens when the socket has disconnected
        // have a callback that detatches this socket from everything
        close_client();
    }
    else {
        _recv_control_msg_callback(req_data, _client_addr);
    }
}

void ClientConnection::can_write_UDP() {
    if (!_chunk_buffer.empty()) {
        auto p = _chunk_buffer.front();
        socklen_t len = sizeof(_client_addr);
        ssize_t nb = sendto(_udp_fd, p.get(), sizeof(FileChunk), 0, 
                            (struct sockaddr*)&_client_addr, len);
        if (nb == -1) {
            std::cerr << "Error while writing to UDP socket " << get_addr_str() << " (errno " << errno << ")" << std::endl;
        }
        else {
            _chunk_buffer.pop();
            _send_chunk_callback(_client_id, p->id);
        }
    }
}
