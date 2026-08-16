#include "stabilization.hpp"
#include "file_entry.hpp"
#include "protocol.hpp"
#include "sha1.hpp"
#include "utils.hpp"
#include "threads.hpp" // For reply_queue and download_queue

#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstdlib>
#include <cstring>

// create new chord ring
void create_ring(Node *n) {
    n->predecessor = nullptr;
    n->successor = n;
    std::cout << "New ring created." << std::endl;
}

// join an existing chord ring
void join_ring(Node *n, const char *existing_ip, const int existing_port) {
    // check if existing node is reachable
    Message msg;
    msg.request_id = generate_id();
    strcpy(msg.type, MSG_HEARTBEAT);
    memcpy(msg.id, n->id, HASH_SIZE);
    strcpy(msg.ip, n->ip);
    msg.port = n->port;
    strcpy(msg.data, "");

    if (send_message(n, existing_ip, existing_port, &msg) < 0) {
        std::cerr << "Unable to reach the existing node at " << existing_ip << ":" << existing_port << std::endl;
        exit(EXIT_FAILURE);
    }

    // send a JOIN message to the existing node
    msg.request_id = generate_id();
    strcpy(msg.type, MSG_JOIN);
    memcpy(msg.id, n->id, HASH_SIZE);
    strcpy(msg.ip, n->ip);
    msg.port = n->port;
    strcpy(msg.data, "");
    send_message(n, existing_ip, existing_port, &msg);

    // the existing node will respond with the new node's successor
    const Message *response = pop_message(&reply_queue, msg.request_id);
    if (response == nullptr) {
        std::cerr << "Failed to join the ring at " << existing_ip << ":" << existing_port << std::endl;
        exit(EXIT_FAILURE);
    }

    // create a new node for the successor
    Node *successor = create_node(response->ip, response->port);

    // set the new node's successor to the one returned by the existing node
    n->predecessor = nullptr;
    n->successor = successor;

    // start retrieving the files metadata from the successor
    Message msg2;
    msg2.request_id = generate_id();
    strcpy(msg2.type, MSG_GET_FILES);
    memcpy(msg2.id, n->id, HASH_SIZE);
    strcpy(msg2.ip, n->ip);
    msg2.port = n->port;
    strcpy(msg2.data, "");

    if (send_message(n, n->successor->ip, n->successor->port, &msg2) < 0) {
        // the successor failed to respond
        std::cout << "Joined the ring at " << n->successor->ip << ":" << n->successor->port << std::endl;
        return;
    }

    char *buf = static_cast<char*>(malloc(4096));
    size_t buf_size = 4096;
    size_t file_data_size = 0;

    // receive the file metadata from the successor
    while ((response = pop_message(&download_queue, msg2.request_id)) != nullptr && strcmp(response->type, MSG_FILE_END) != 0) {
        const size_t segment_start = response->segment_index * sizeof(response->data);
        const size_t segment_end = segment_start + response->data_len;

        if (segment_end > buf_size) {
            buf = static_cast<char*>(realloc(buf, segment_end));
            buf_size = segment_end;
        }

        memcpy(buf + segment_start, response->data, response->data_len);

        if (response->segment_index + 1 == response->total_segments) {
            file_data_size = segment_end;
        }
    }

    if (file_data_size > 0 && response != nullptr && response->segment_index + 1 == response->total_segments) {
        deserialize_file_entries(n, buf, file_data_size);
    }

    std::cout << "Joined the ring at " << n->successor->ip << ":" << n->successor->port << std::endl;

    free(buf);
}

void stabilize(Node *n) {
    // send a STABILIZE message to the successor
    Message msg;
    msg.request_id = generate_id();
    strcpy(msg.type, MSG_STABILIZE);
    memcpy(msg.id, n->id, HASH_SIZE);
    strcpy(msg.ip, n->ip);
    msg.port = n->port;
    strcpy(msg.data, "");
    send_message(n, n->successor->ip, n->successor->port, &msg);

    // receive the successor's predecessor to verify
    const Message *response = pop_message(&reply_queue, msg.request_id);
    if(response == nullptr) return;

    // x is the successor's predecessor
    Node *x = create_node(response->ip, response->port);

    // if x is in the interval (n, successor), then update the successor
    if (is_in_interval(x->id, n->id, n->successor->id)) {
        cleanup_node(n->successor); // Assuming cleanup_node deallocates
        n->successor = x;
    } else {
        cleanup_node(x);
    }

    // notify the new successor to update its predecessor
    notify(n->successor, n);
}

void notify(Node *n, Node *n_prime) {
    // send a NOTIFY message to n
    Message msg;
    msg.request_id = generate_id();
    strcpy(msg.type, MSG_NOTIFY);
    memcpy(msg.id, n_prime->id, HASH_SIZE);
    strcpy(msg.ip, n_prime->ip);
    msg.port = n_prime->port;
    strcpy(msg.data, "");

    // notify that n_prime might be its new predecessor
    send_message(n_prime, n->ip, n->port, &msg);

    if (n->predecessor == nullptr || is_in_interval(n_prime->id, n->predecessor->id, n->id)) {
        if(n->predecessor != nullptr) cleanup_node(n->predecessor);
        n->predecessor = create_node(n_prime->ip, n_prime->port);
        memcpy(n->predecessor->id, n_prime->id, HASH_SIZE);
    }
}

void fix_fingers(Node *n, int *next) {
    *next = (*next + 1) % M;
    // Calculate the ID to find the successor for
    // This part is complex and depends on the Chord specification for finger tables.
    // Assuming n->finger[*next]->id is correctly populated.
    Node *finger = find_successor(n, n->finger[*next]->id);
    if (n->finger[*next] != n) { // Don't clean up the node itself
        cleanup_node(n->finger[*next]);
    }
    n->finger[*next] = finger;
}

void check_predecessor(Node *n) {
    if (n->predecessor == nullptr) {
        return;
    }

    // send a heartbeat message to the predecessor
    Message msg;
    msg.request_id = generate_id();
    strcpy(msg.type, MSG_HEARTBEAT);
    memcpy(msg.id, n->id, HASH_SIZE);
    strcpy(msg.ip, n->ip);
    msg.port = n->port;
    strcpy(msg.data, "");

    // if the predecessor does not respond, it has failed -> set it to NULL
    if (send_message(n, n->predecessor->ip, n->predecessor->port, &msg) < 0) {
        cleanup_node(n->predecessor);
        n->predecessor = nullptr;
    }
}

Node *find_successor(Node *n, const uint8_t *id) {
    if (is_in_interval(id, n->id, n->successor->id) || memcmp(id, n->successor->id, HASH_SIZE) == 0) {
        Node* succ = create_node(n->successor->ip, n->successor->port);
        memcpy(succ->id, n->successor->id, HASH_SIZE);
        return succ;
    }

    const Node *current = closest_preceding_node(n, id);
    return find_successor_remote(n, current, id);
}

Node *find_successor_remote(const Node *n, const Node *n0, const uint8_t *id) {
    // Avoid sending message to itself
    if (n0 == n) {
        Node* succ = create_node(n->successor->ip, n->successor->port);
        memcpy(succ->id, n->successor->id, HASH_SIZE);
        return succ;
    }

    // send a FIND_SUCCESSOR message to the current node
    Message msg;
    msg.request_id = generate_id();
    strcpy(msg.type, MSG_FIND_SUCCESSOR);
    memcpy(msg.id, id, HASH_SIZE);
    strcpy(msg.ip, n->ip);
    msg.port = n->port;
    strcpy(msg.data, "");

    send_message(n, n0->ip, n0->port, &msg);

    // receive the successor's info
    const Message *response = pop_message(&reply_queue, msg.request_id);
    
    if(response == nullptr) {
        // Handle timeout or error. Returning self's successor as a fallback.
        Node* succ = create_node(n->successor->ip, n->successor->port);
        memcpy(succ->id, n->successor->id, HASH_SIZE);
        return succ;
    }

    Node *successor = create_node(response->ip, response->port);
    memcpy(successor->id, response->id, HASH_SIZE);
    
    return successor;
}

Node *closest_preceding_node(Node *n, const uint8_t *id) {
    // search the finger table in reverse order to find the closest node
    for (int i = M - 1; i >= 0; i--) {
        if (n->finger[i] != nullptr && is_in_interval(n->finger[i]->id, n->id, id)) {
            return n->finger[i];
        }
    }
    return n;
}
