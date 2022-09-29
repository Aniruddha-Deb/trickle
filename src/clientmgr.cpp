#include "client.hpp"
#include <csignal>
#include <thread>
#include <unistd.h>
#include <getopt.h>
#include <chrono>
#include <atomic>

using namespace std::placeholders;
using namespace std::literals;

volatile bool running = true;
std::atomic<int> n_completed_ctr;
int n;

void terminate(int signal) {
    running = false;
}

void print_usage() {
    std::cout << "usage: clientmgr [-a address] [-p port] [-o output_folder] num_clients" << std::endl;
}

void file_saved() {
    n_completed_ctr++;
    if (n_completed_ctr == n) running = false;
}

int main(int argc, char** argv) {

    n_completed_ctr = 0;

    std::signal(SIGINT, terminate);
    std::signal(SIGTERM, terminate);
    std::signal(SIGKILL, terminate);

    std::string addr = "127.0.0.1";
    int port = 15000;
    std::string out_folder = ".";

    char opt;
    while ((opt = getopt(argc, argv, "a:p:o:")) != -1) {
        std::cout << "Got opt " << opt << " with arg " << std::string(optarg) << std::endl;
        switch (opt) {
            case 'a': addr = std::string(optarg); break;
            case 'p': port = std::stoi(std::string(optarg)); break;
            case 'o': out_folder = std::string(optarg); break;
            default:
                print_usage();
                return 0;
        }
    }

    if (optind >= argc) {
        std::cout << "clientmgr: missing required argument num_clients" << std::endl;
        print_usage();
        return 0;
    }

    // spawn multiple threads and assign one to each client

    n = std::stoi(std::string(argv[argc-1]));
    std::vector<std::thread> threads(n);
    std::vector<std::unique_ptr<Client>> clients(n);


    for (int i=0; i<n and running; i++) {
        std::this_thread::sleep_for(200ms);
        clients[i] = std::unique_ptr<Client>(new Client(addr, port, out_folder));
        clients[i]->on_save_file(file_saved);
        threads[i] = std::thread(&Client::run, clients[i].get(), std::ref(running));
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i=0; i<n; i++) {
        // wait for everyone to finish 
        threads[i].join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    std::cout << "All clients received file in " << (end_time-start_time)/1ms << " ms" << std::endl;

    return 0;
}