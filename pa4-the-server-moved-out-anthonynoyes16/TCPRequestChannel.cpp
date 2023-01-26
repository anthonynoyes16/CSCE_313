#include "TCPRequestChannel.h"

using namespace std;

// man socket
TCPRequestChannel::TCPRequestChannel (const std::string _ip_address, const std::string _port_no) {
    // use ip address to determine server/client
    // if server
    if(_ip_address == "") {
        struct sockaddr_in server;
       
        server.sin_family = AF_INET; // AF_INET = IPv4 (domain)
        server.sin_port = htons(stoi(_port_no));
        server.sin_addr.s_addr = INADDR_ANY; // ip address is blank, use whatever we have right now
        // create a socket on the specified port
        int sock = socket(AF_INET, SOCK_STREAM, 0); // SOCK_STREAM = TCP
        this->sockfd = sock;
        if (sock < 0) {
            perror("socket failed server");
        }
        
        // bind socket to address using get address info, set up listening
        if (bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
            perror("bind failed server");
        }

        // mark the socket as listening
        if(listen(sock, 50) < 0) {
            perror("listen failed server");
        }  
        // int accepted = accept_conn(); // do this in the server
    }
    // if client
    else {
        struct sockaddr_in server_info;
        // int client_sock, connect_stat;
        // create a socket on the specified port
        server_info.sin_family = AF_INET; // AF_INET = IPv4
        server_info.sin_port = htons(stoi(_port_no));
        // server_info.sin_addr.s_addr = INADDR_ANY;
        int sock = socket(AF_INET, SOCK_STREAM, 0); // SOCK_STREAM = TCP  
        this->sockfd = sock; 
        if (sock < 0 ){
            perror("socket failed client");
        }     
        inet_aton(_ip_address.c_str(), &server_info.sin_addr);
        // inet_pton(AF_INET, _ip_address.c_str(), &server_info);
        // connect socket to IP address of the server
        if (connect(sock, (struct sockaddr*)&server_info, sizeof(server_info)) < 0) {
            // printf("sock is: %d\n", sock);
            perror("conenct failed");
        }
    }
            
}

TCPRequestChannel::TCPRequestChannel (int _sockfd) {
    this->sockfd = _sockfd;
}

TCPRequestChannel::~TCPRequestChannel () {
    // close the sockfd
    close(this->sockfd);
}

int TCPRequestChannel::accept_conn () {
    struct sockaddr_in connect_to;
    socklen_t connect_to_size = sizeof(connect_to);
    int accept_return = accept(this->sockfd, (struct sockaddr*)&connect_to, &connect_to_size);
    // printf("accept return is: %d", accept_return);
    if (accept_return < 0) {
        perror("accept failed");
    }
    return accept_return;
     
    // implementing accept(...) - return socket fd of the client

}

// read/write, recv/send
int TCPRequestChannel::cread (void* msgbuf, int msgsize) {
    // read(int fd, void *buf, size_t count)
    return read(this->sockfd, msgbuf, (size_t)msgsize);
    // return number of bytes read
}

int TCPRequestChannel::cwrite (void* msgbuf, int msgsize) {
    // write(int fd, void *buf, size_t count)
    return write(this->sockfd, msgbuf, (size_t)msgsize);
    // return number of bytes written
}
