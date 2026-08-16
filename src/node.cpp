#include "node.hpp"
#include "file_entry.hpp"
#include "stabilization.hpp"
#include "sha1.hpp"
#include "utils.hpp"
#include "threads.hpp"

#include <cstdlib>
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

Node *create_node(const char *ip, const int port) {
    Node *node = new Node;
    char id_str[256];
    snprintf(id_str, sizeof(id_str), "%s:%d", ip, port);
    hash(id_str, node->id);
    strncpy(node->ip, ip, sizeof(node->ip) - 1);
    node->ip[sizeof(node->ip) - 1] = '\0';
    node->port = port;
    node->successor = node;
    node->predecessor = nullptr;
    node->files = nullptr;
    node->uploaded_files = nullptr;
    for (int i = 0; i < M; i++) {
        node->finger[i] = node; // initially, all fingers point to the node itself
    }
    node->socket_open = false;
    return node;
}

void node_bind(Node *n) {
    if ((n->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        delete n;
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    if (setsockopt(n->sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0) {
        perror("setsockopt");
        close(n->sockfd);
        delete n;
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(n->port);
    addr.sin_addr.s_addr = inet_addr(n->ip);

    if (bind(n->sockfd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(n->sockfd);
        delete n;
        exit(EXIT_FAILURE);
    }

    n->socket_open = true;
}

void cleanup_node(Node *n) {
    if(n == nullptr) return;

    Message msg;
    if (n->successor != n) {
        // ... (Logic for notifying network about leaving)
        // This part needs careful implementation of transferring files and updating neighbors.
    }

    // Free files
    FileEntry *file = n->files;
    while (file != nullptr) {
        FileEntry *next = file->next;
        delete file;
        file = next;
    }
    
    // Free uploaded files list
    file = n->uploaded_files;
    while (file != nullptr) {
        FileEntry *next = file->next;
        delete file;
        file = next;
    }

    // Clean up
    if(n->socket_open) {
        close(n->sockfd);
    }
    
    // In a real scenario, you need to be careful not to delete nodes that are still pointed to by others.
    // This simple delete is for standalone node cleanup.
    // delete n;
}

void handle_requests(Node *n, const Message *msg) {
    if (strcmp(msg->type, MSG_FILE_DATA) == 0 || strcmp(msg->type, MSG_FILE_END) == 0) {
        push_message(&download_queue, msg);
    } else if (strcmp(msg->type, MSG_REPLY) == 0) {
        push_message(&reply_queue, msg);
    } else if (strcmp(msg->type, MSG_JOIN) == 0) {
        Node *new_node = create_node(msg->ip, msg->port);
        const Node *successor = find_successor(n, new_node->id);

        Message response;
        response.request_id = msg->request_id;
        strcpy(response.type, MSG_REPLY);
        memcpy(response.id, successor->id, HASH_SIZE);
        strcpy(response.ip, successor->ip);
        response.port = successor->port;
        strcpy(response.data, "");

        send_message(n, msg->ip, msg->port, &response);
        delete new_node;
        delete successor;
    } else if (strcmp(msg->type, MSG_FIND_SUCCESSOR) == 0) {
        const Node *successor = find_successor(n, msg->id);

        Message response;
        response.request_id = msg->request_id;
        strcpy(response.type, MSG_REPLY);
        memcpy(response.id, successor->id, HASH_SIZE);
        strcpy(response.ip, successor->ip);
        response.port = successor->port;
        strcpy(response.data, "");

        send_message(n, msg->ip, msg->port, &response);
        delete successor;
    } else if (strcmp(msg->type, MSG_GET_FILES) == 0) {
        Message response;
        response.request_id = msg->request_id;
        memcpy(response.id, n->id, HASH_SIZE);

        char *buf = static_cast<char*>(malloc(4096));
        size_t data_size = serialize_file_entries(&buf, 4096, n->files, msg->id, n->id);

        const size_t chunk_size = sizeof(response.data);
        size_t total_chunks = (data_size + chunk_size - 1) / chunk_size;

        strcpy(response.type, MSG_FILE_DATA);
        for (size_t i = 0; i < total_chunks; i++) {
            response.segment_index = static_cast<uint16_t>(i);
            response.total_segments = static_cast<uint16_t>(total_chunks);
            size_t start_index = i * chunk_size;
            size_t end_index = (start_index + chunk_size > data_size) ? data_size : start_index + chunk_size;
            size_t segment_size = end_index - start_index;
            memcpy(response.data, buf + start_index, segment_size);
            response.data_len = segment_size;
            send_message(n, msg->ip, msg->port, &response);
        }

        strcpy(response.type, MSG_FILE_END);
        strcpy(response.data, "");
        response.data_len = 0;
        send_message(n, msg->ip, msg->port, &response);

        delete_transferred_files(&n->files, msg->id, n->id);
        free(buf);
    } else if (strcmp(msg->type, MSG_STABILIZE) == 0) {
        if (n->predecessor != nullptr) {
            Message response;
            response.request_id = msg->request_id;
            strcpy(response.type, MSG_REPLY);
            memcpy(response.id, n->predecessor->id, HASH_SIZE);
            strcpy(response.ip, n->predecessor->ip);
            response.port = n->predecessor->port;
            strcpy(response.data, "");
            send_message(n, msg->ip, msg->port, &response);
        }
    } else if (strcmp(msg->type, MSG_NOTIFY) == 0) {
        Node *new_predecessor = create_node(msg->ip, msg->port);
        memcpy(new_predecessor->id, msg->id, HASH_SIZE);

        if (n->predecessor == nullptr || is_in_interval(new_predecessor->id, n->predecessor->id, n->id)) {
            delete n->predecessor;
            n->predecessor = new_predecessor;
        } else {
            delete new_predecessor;
        }
    }
    // ... other message handlers
     else {
        std::cout << "Unknown message type: " << msg->type << std::endl;
    }
}
