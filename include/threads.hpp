#ifndef THREADS_HPP
#define THREADS_HPP

// Forward declarations to avoid including the full header
struct Node;
struct MessageQueue;

extern MessageQueue reply_queue;
extern MessageQueue download_queue;

void *node_thread(void *arg);

void *listener_thread(void *arg);

void handle_user_commands(Node *node);

void handle_exit(int sig);

#endif //THREADS_HPP
