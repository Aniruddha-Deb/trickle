#include "server.hpp"
#include <csignal>
#include <iostream>
#include <string>
#include <unistd.h>
#include <getopt.h>

volatile bool running = true;

void terminate(int signal) {
    running = false;
}

// args:
// 1. Server port
// 2. Min no. clients
// 3. LRU cache size (in kB)

void print_usage() {
    std::cout << "usage: server_(tcp|udp) [-n num_clients] [-c cache_size] [-p port] file_to_share" << std::endl;
}

int main(int argc, char** argv) {

    std::signal(SIGINT, terminate);
    std::signal(SIGTERM, terminate);
    std::signal(SIGKILL, terminate);

    int n_clients = 5;
    int cache_size = 100;
    int port = 15000;

    char opt;
    while ((opt = getopt(argc, argv, "n:p:c:")) != -1) {
        std::cout << "Got opt " << opt << " with arg " << std::string(optarg) << std::endl;
        switch (opt) {
            case 'n': n_clients = std::stoi(std::string(optarg)); break;
            case 'p': port = std::stoi(std::string(optarg)); break;
            case 'c': cache_size = std::stoi(std::string(optarg)); break;
            default:
                print_usage();
                return 0;
        }
    }

    if (optind >= argc) {
        std::cout << "server: missing required argument file_to_share" << std::endl;
        print_usage();
        return 0;
    }

    Server srv(port, n_clients, cache_size);
    srv.load_file(std::string(argv[argc-1]));
    srv.run(running);

    return 0;
}
