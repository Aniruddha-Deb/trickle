#include <sys/socket.h>
#include <errno.h>

#include "server.hpp"

void Server::can_read_UDP() {
    ControlMessage req_data;
    struct sockaddr_in sender;
    socklen_t len = sizeof(sender);
    int nb = recvfrom(_udp_ss, &req_data, sizeof(req_data), 0, (struct sockaddr*)&sender, &len);

    if (nb == -1) {
        std::cerr << "Error while reading from UDP socket (errno " << errno << ")" << std::endl;
    }
    else {
        received_control_msg(req_data, sender);
    }
}