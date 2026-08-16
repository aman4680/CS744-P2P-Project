#include "protocol.hpp"
#include "node.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

void init_queue(MessageQueue *queue) {
    queue->head = queue->tail = nullptr;
    pthread_mutex_init(&queue->mutex, nullptr);
    pthread_cond_init(&queue->cond, nullptr);
}

void push_message(MessageQueue *queue, const Message *msg) {
    MessageNode *new_node = new MessageNode;
    new_node->msg = new Message;
    memcpy(new_node->msg, msg, sizeof(Message));
    new_node->next = nullptr;

    pthread_mutex_lock(&queue->mutex);
    if (queue->tail == nullptr) {
        queue->head = queue->tail = new_node;
    } else {
        queue->tail->next = new_node;
        queue->tail = new_node;
    }
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

Message *pop_message(MessageQueue *queue, const uint32_t request_id) {
    pthread_mutex_lock(&queue->mutex);

    MessageNode *prev = nullptr;
    MessageNode *current = queue->head;

    while (true) {
        while (current != nullptr) {
            if (current->msg->request_id == request_id) {
                if (prev) {
                    prev->next = current->next;
                } else {
                    queue->head = current->next;
                }
                if (current == queue->tail) {
                    queue->tail = prev;
                }
                
                Message *msg = current->msg;
                delete current;
                pthread_mutex_unlock(&queue->mutex);
                return msg;
            }
            prev = current;
            current = current->next;
        }
        // If no message found, wait for a signal
        pthread_cond_wait(&queue->cond, &queue->mutex);
        // After wake-up, restart search from the head
        current = queue->head;
        prev = nullptr;
    }
}


int send_message(const Node *sender, const char *receiver_ip, const int receiver_port, const Message *msg) {
    struct sockaddr_in receiver_addr = {0};
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(receiver_port);
    receiver_addr.sin_addr.s_addr = inet_addr(receiver_ip);

    char buffer[sizeof(Message)];
    memcpy(buffer, msg, sizeof(Message));

    return sendto(sender->sockfd, buffer, sizeof(Message), 0,
                  reinterpret_cast<struct sockaddr *>(&receiver_addr), sizeof(receiver_addr));
}

int receive_message(const Node *n, Message *msg) {
    if (!n->socket_open) {
        usleep(10000); // Sleep for 10ms to avoid busy waiting
        return -1;
    }

    struct sockaddr_in sender_addr = {0};
    socklen_t sender_addr_len = sizeof(sender_addr);

    char buffer[sizeof(Message)];
    const int bytes_read = recvfrom(n->sockfd, buffer, sizeof(Message), 0,
                                    reinterpret_cast<struct sockaddr *>(&sender_addr), &sender_addr_len);

    if (bytes_read < 0) {
        perror("recvfrom failed");
        return -1;
    }
    
    if (bytes_read == sizeof(Message)) {
        memcpy(msg, buffer, sizeof(Message));
    } else {
        // Handle partial receive or error
        return -1;
    }

    return bytes_read;
}
