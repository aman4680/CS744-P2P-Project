#ifndef NODE_HPP
#define NODE_HPP

#include <cstdint>
#include <cstdbool>

#define M 160 // number of bits in the hash (SHA-1)

// Forward declarations
struct FileEntry;
struct Message;

struct Node {
    uint8_t id[20]; // SHA-1 hash
    char ip[16];
    int port;
    Node *successor;
    Node *predecessor;
    Node *finger[M];
    FileEntry *files;
    FileEntry *uploaded_files;
    int sockfd;
    bool socket_open;
};

#include "protocol.hpp"

Node *create_node(const char *ip, int port);

void node_bind(Node *n);

void cleanup_node(Node *n);

void handle_requests(Node *n, const Message *msg);

#endif //NODE_HPP
