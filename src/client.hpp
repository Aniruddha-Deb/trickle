#pragma once

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <string>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <errno.h>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <random>
#include <memory>
#include <fstream>
#include <chrono>
#include <functional>

#include "event_queue.hpp"
#include "protocol.hpp"

using namespace std::literals;

using udpaddr_t = struct sockaddr_in;

extern volatile bool running;

class Client {

    uintptr_t _tcp_sock;
    uintptr_t _udp_sock;

    // TODO how do we claim this won't clash with any other file descriptor?
    static const uintptr_t TIMER_FD = 65535;

    std::unordered_map<uint32_t, std::chrono::time_point<std::chrono::high_resolution_clock>>
        _chunk_request_times;
    std::unordered_map<uint32_t, std::chrono::microseconds> 
        _chunk_rtt_times;

    EventQueue _evt_queue;

    bool _registered = false;
    bool _saved_file = false;
    bool _can_request = false;

    uint32_t _client_id;
    uint32_t _num_chunks;
    uint32_t _num_rcvd_chunks = 0;

    std::vector<std::unique_ptr<FileChunk>> _chunks;
    std::vector<bool> _rcvd_chunks;

    std::queue<uint32_t> _chunk_buffer;
    std::queue<ControlMessage> _control_msg_buffer;

    std::queue<std::unique_ptr<FileChunk>> _recv_chunk_cache;

    // chunk requests may be randomized across clients while testing
    // solution: have a random permutation of chunk id's that we'll use and then
    // sequentially go over them. If we have the chunk, then ignore. 
    std::vector<uint32_t> _req_sequence;

    size_t _next_chunk_idx = 0;

    std::string _output_folder;
    std::string _rtt_file_name;

    std::function<void(void)> _save_file_callback;
    bool _save_file_callback_bound = false;

public:

    Client(std::string server_addr, uint16_t server_port, std::string output_folder):
           _output_folder{output_folder} {

        struct addrinfo* dest_addr = addr_info(server_addr, server_port, SOCK_STREAM);
        _tcp_sock = create_socket(dest_addr);

        if (_tcp_sock == -1) {
            std::cout << "Could not make TCP socket" << std::endl;
            return;
        }

        configure_tcp(_tcp_sock);

        // connect TCP socket
        int err = connect(_tcp_sock, dest_addr->ai_addr, dest_addr->ai_addrlen);
        if (err == -1) {
            std::cout << "Could not connect socket (errno " << errno << ")" << std::endl;
            running = false;
            return;
            //err = connect(_tcp_sock, dest_addr->ai_addr, dest_addr->ai_addrlen);
        }

        // get TCP socket local address
        struct sockaddr_in src_addr;
        socklen_t len = sizeof(src_addr);
        getsockname(_tcp_sock, (struct sockaddr*)&src_addr, &len);

        // we use this to initialize UDP, as we want it to be bound at the same
        // port as TCP is
        dest_addr->ai_socktype = SOCK_DGRAM;
        dest_addr->ai_protocol = IPPROTO_UDP;
        _udp_sock = create_socket(dest_addr);

        if (_udp_sock == -1) {
            std::cout << "Could not make UDP socket" << std::endl;
            return;
        }

        err = bind(_udp_sock, (struct sockaddr*)&src_addr, len);
        if (err == -1) {
            std::cout << "Could not bind UDP socket (errno " << errno << ")" << std::endl;
            running = false;
            return;
        }

        // connect UDP socket now
        err = connect(_udp_sock, dest_addr->ai_addr, dest_addr->ai_addrlen);
        if (err == -1) {
            std::cout << "Could not connect socket (errno " << errno << ")" << std::endl;
            running = false;
            return;
        }

        // set socket(s) to be non-blocking now
        fcntl(_tcp_sock, F_SETFL, O_NONBLOCK);
        fcntl(_udp_sock, F_SETFL, O_NONBLOCK);

        setup_evt_queue();

        std::cout << "Created client and connected to server" << std::endl;
    }

    void on_save_file(std::function<void(void)> save_file_callback) {
        _save_file_callback = save_file_callback;
        _save_file_callback_bound = true;
    }


    struct addrinfo* addr_info(std::string& addr, uint16_t port, int socktype) {
        struct addrinfo hints;
        struct addrinfo *res;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = socktype;
        getaddrinfo(addr.c_str(), std::to_string(port).c_str(), &hints, &res);

        return res;
    }

    uintptr_t create_socket(struct addrinfo* res) {

        if (res->ai_socktype == SOCK_STREAM) {
            std::cout << "Creating TCP socket" << std::endl;
        }
        else if (res->ai_socktype == SOCK_DGRAM) {
            std::cout << "Creating UDP socket" << std::endl;
        }
        else {
            std::cout << "res: ai_protocol doesn't exist" << std::endl;
        }

        uintptr_t fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd == -1) {
            std::cout << "ERROR: failed to allocate socket (errno " << errno << ")" << std::endl;
            return -1;
        }
        else {
            std::cout << "Got socket (fd = " << fd << ")" << std::endl;
        }
        return fd;
    }

    void setup_evt_queue() {
        _evt_queue.add_event(_tcp_sock, EVFILT_READ);
        _evt_queue.add_event(_tcp_sock, EVFILT_WRITE);
        _evt_queue.add_event(_udp_sock, EVFILT_READ);
        _evt_queue.add_event(_udp_sock, EVFILT_WRITE);
    }

    void init_chunk_request_sequence(bool randomize) {
        _req_sequence.resize(_num_chunks);
        _chunks.resize(_num_chunks);
        _rcvd_chunks.resize(_num_chunks, false);

        std::cout << "There are " << _num_chunks << " chunks making up the file" << std::endl;

        for (uint32_t i=0; i<_num_chunks; i++) {
            _req_sequence[i] = i;
            // _chunks[i] = nullptr; // TODO zero check here
        }

        if (randomize) {
            std::random_device rd;
            std::seed_seq seed{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
            std::mt19937 rng{seed};

            std::shuffle(_req_sequence.begin(), _req_sequence.end(), rng);
        }
    }

    void save_file() {
        // TODO incorporate passing metadata in the zeroth chunk eg. name of file
        // for now, we save everything as text files
        // (definetly not a good thing)
        std::cout << "Saving file" << std::endl;
        std::string outfile = _output_folder+"/outfile_"+std::to_string(_client_id)+".txt";
        std::ofstream f(outfile);

        for (int i=0; i<_num_chunks; i++) {
            f.write(_chunks[i]->data, _chunks[i]->size);
        }

        f.close();
        if (_save_file_callback_bound) {
            _save_file_callback();
        }

        std::cout << "Received entire file, wrote to " << outfile << std::endl;        
    }

    void save_RTT_times() {
        std::ofstream out(_output_folder+"/rtt_"+std::to_string(_client_id)+".csv");
        for (auto p : _chunk_rtt_times) {
            out << p.first << "," << p.second.count() << std::endl;
        }

        out.close();
    }

    // implementation defined
    void configure_tcp(uintptr_t tcp_fd);

    void can_read_TCP();

    void can_read_UDP();

    void can_write_TCP();

    void can_write_UDP();

    // callbacks

    void sent_chunk(uint32_t chunk_id) {
        // do nothing for now :P
        std::cout << "Sent chunk " << chunk_id << std::endl;
    }

    void received_chunk(std::unique_ptr<FileChunk>&& chunk) {

        // If we receive a chunk while we're not registered, what do we do?
        // Solution: cache the chunk, and once we're registered, insert the 
        // chunks in the cache into the chunk map
        //
        // why don't we just push this to chunks the minute we receive it?
        // Because to init the chunks, we need the number of chunks. 

        if (!_registered) {
            _recv_chunk_cache.push(std::move(chunk));
        }
        else {
            const auto chunk_id = chunk->id;
            if (!_rcvd_chunks[chunk_id]) {                
                _chunks[chunk_id] = std::move(chunk);
                _rcvd_chunks[chunk_id] = true;
                if (_chunk_request_times.find(chunk_id) != _chunk_request_times.end()) {
                    auto curr_time = std::chrono::high_resolution_clock::now();
                    _chunk_rtt_times[chunk_id] = 
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            curr_time - _chunk_request_times[chunk_id]
                        );
                    _chunk_request_times.erase(chunk_id);
                }
                _num_rcvd_chunks++;
                std::cout << "Received chunk " << chunk_id << ", have " << _num_rcvd_chunks << " chunks now." << std::endl;
            }

            if (_num_rcvd_chunks == _num_chunks and !_saved_file) {
                save_file();
                save_RTT_times();
                _saved_file = true;
            }
        }
    }

    void disconnected() {
        // Let's assume abnormal disconnects on TCP are rare, and this is only
        // called in the event that the server bumps us off. 

        // simply set running to false and disconnect.

        std::cout << "Disconnected from server" << std::endl;
        running = false;

    }

    void clear_chunk_cache() {
        while (!_recv_chunk_cache.empty()) {
            received_chunk(std::move(_recv_chunk_cache.front()));
            _recv_chunk_cache.pop();
        }
    }

    void received_control(ControlMessage& m) {
        if (m.msgtype == OPEN) {
            _can_request = true;
        }
        else if (m.msgtype == REG and !_registered) {
            std::cout << "Reading registration data" << std::endl;
            _client_id = m.client_id;
            _num_chunks = m.chunk_id;
            _registered = true;

            init_chunk_request_sequence(false);
            clear_chunk_cache();
            std::cout << "registered with client_id " << _client_id << std::endl;
        }
        else if (_registered and m.msgtype == REQ and _chunks[m.chunk_id] != nullptr) {
            send_chunk(m.chunk_id);
        }
    }

    // send requests/queue

    void request_chunk(uint32_t chunk_id) {
        _control_msg_buffer.push({REQ,_client_id,chunk_id});
    }

    void send_chunk(uint32_t chunk_id) {
        _chunk_buffer.push(chunk_id);
    }

    void periodic_resend_requests() {
        auto curr_time = std::chrono::high_resolution_clock::now();

        if (!_registered) {
            // send a registration request to the server; the registration packet
            // might have been lost somewhere
            std::cout << "Not registered, trying to send a request" << std::endl;
            _control_msg_buffer.push({REG,0,0});
        }
        else if (!_can_request and _registered) {
            std::cout << "Checking if can request" << std::endl;
            _control_msg_buffer.push({OPEN,_client_id,0});
        }
        else if (_registered and _can_request) {

            // for (auto p : _chunk_request_times) {
            //     std::cout << p.first << "," << (curr_time-p.second)/1ms << std::endl;
            // }

            for (auto it = _chunk_request_times.cbegin(); it != _chunk_request_times.cend(); ) {
                if ((curr_time-it->second)/1ms > 5000) {
                    // re-request chunk
                    _req_sequence.push_back(it->first);
                    _chunk_request_times.erase(it++);
                    continue;
                }     
                else {
                    ++it;
                }
            }
        }
    }

    void run(volatile bool& running) {
        _evt_queue.add_timer_event(TIMER_FD, 1000);

        while (running) {

            std::vector<event_t> evts = _evt_queue.get_events();

            for (const auto& e : evts) {
                if (e.ident == _tcp_sock) {
                    if (e.filter == EVFILT_READ) can_read_TCP();
                    if (e.filter == EVFILT_WRITE) can_write_TCP();
                }
                else if (e.ident == _udp_sock) {
                    if (e.filter == EVFILT_READ) can_read_UDP();
                    else if (e.filter == EVFILT_WRITE) can_write_UDP();
                }
                else if (e.ident == TIMER_FD and e.filter == EVFILT_TIMER) {
                    periodic_resend_requests();
                }
            }

            // have a cap of 100 so that we don't overburden the network with 
            // too many requests at once
            if (_can_request and _registered and _chunk_request_times.size() < 100) {

                // request the chunks we don't have on UDP, once in every loop
                while (_next_chunk_idx < _req_sequence.size() and 
                       _chunks[_req_sequence[_next_chunk_idx]] != nullptr) {
                    _next_chunk_idx++;
                }
                if (_next_chunk_idx < _req_sequence.size()) {
                    // get and store server udp socket id 
                    // would just be server tcp socket id + 1
                    //std::cout << "Requesting chunk " << _req_sequence[_next_chunk_idx] << std::endl;
                    //std::cout << _chunks[_req_sequence[_next_chunk_idx]] <<
                    request_chunk(_req_sequence[_next_chunk_idx]);
                    _chunk_request_times[_req_sequence[_next_chunk_idx]] = 
                        std::chrono::high_resolution_clock::now();
                    _next_chunk_idx++;
                }
                // The timer takes care of this
                // if (_next_chunk_idx >= _req_sequence.size()) {
                //     std::cout << "Requested all chunks in sequence" << std::endl;
                //     std::cout << "Exiting without receiving complete file" << std::endl;
                //     break;
                // }
            }

        }

        if (!_saved_file) {
            save_RTT_times();
        }

        // shutdown things here
        _evt_queue.close();
        close(_tcp_sock);
        close(_udp_sock);
    }

};
