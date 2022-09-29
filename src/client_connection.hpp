#pragma once

#include <queue>
#include <memory>
#include <string>
#include <functional>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>


#include "protocol.hpp"

class ClientConnection {

    uint32_t _client_id;
    uintptr_t _tcp_fd;
    uintptr_t _udp_fd;
    struct sockaddr_in _client_addr;

    std::function<void(std::shared_ptr<FileChunk>)> _recv_chunk_callback;
    std::function<void(uint32_t, uint32_t)> _send_chunk_callback;
    std::function<void(ControlMessage, struct sockaddr_in)> _recv_control_msg_callback;
    std::function<void(uint32_t)> _disconnect_callback;

    std::queue<ControlMessage> _control_msg_buffer;
    std::queue<std::shared_ptr<FileChunk>> _chunk_buffer;

public:

    ClientConnection(uint32_t client_id, uintptr_t tcp_fd, uintptr_t udp_fd, struct sockaddr_in client_addr);

    // These four methods are implementation specific: the client(s) can choose
    // whether they want to share control over a TCP layer or over a UDP layer

    void can_write_TCP();

    void can_read_TCP();

    void can_write_UDP();

    void on_recv_chunk(std::function<void(std::shared_ptr<FileChunk>)> cb) {
        _recv_chunk_callback = cb;
    }

    void on_send_chunk(std::function<void(uint32_t, uint32_t)> cb) {
        _send_chunk_callback = cb;
    }

    void on_recv_control_msg(std::function<void(ControlMessage,struct sockaddr_in)> cb) {
        _recv_control_msg_callback = cb;
    }

    void on_disconnect(std::function<void(uint32_t)> cb) {
        _disconnect_callback = cb;
    }

    void send_control_msg(ControlMessage msg) {
        _control_msg_buffer.push(msg);
    }

    void req_chunk(uint32_t chunk_id) {
        _control_msg_buffer.push({ REQ, 0, chunk_id });
    }

    void send_chunk(std::shared_ptr<FileChunk> chunk) {
        _chunk_buffer.push(chunk);
    }

    void close_client() {
        _disconnect_callback(_client_id);
        close(_tcp_fd);
    }

    uintptr_t get_tcp_fd() {
        return _tcp_fd;
    }

    uintptr_t get_udp_fd() {
        return _udp_fd;
    }

    uint32_t get_client_id() {
        return _client_id;
    }

    std::string get_addr_str() {
        char s[INET6_ADDRSTRLEN];
        inet_ntop(_client_addr.sin_family, &(_client_addr.sin_addr), s, sizeof s);
        std::string retval(s);
        retval += ":" + std::to_string(ntohs(_client_addr.sin_port));
        return retval;        
    }

    struct sockaddr_in get_addr() {
        return _client_addr;
    }
};
