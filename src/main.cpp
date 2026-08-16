#include "threads.hpp"
#include "stabilization.hpp"
#include "node.hpp"
#include "protocol.hpp"

#include <iostream>
#include <pthread.h>
#include <csignal>
#include <cstdlib>

int main(int argc, char *argv[]) {
    init_queue(&reply_queue);
    init_queue(&download_queue);

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <IP> <PORT> [EXISTING_IP] [EXISTING_PORT]" << std::endl;
        return EXIT_FAILURE;
    }

    signal(SIGINT, handle_exit); // gracefully handle ctrl+c

    const char *ip = argv[1];
    const int port = atoi(argv[2]);

    Node *node = create_node(ip, port);
    node_bind(node);

    pthread_t listener_tid;
    if (pthread_create(&listener_tid, nullptr, listener_thread, node) != 0) {
        perror("pthread_create for listener");
        return EXIT_FAILURE;
    }

    if (argc == 5) {
        const char *existing_ip = argv[3];
        const int existing_port = atoi(argv[4]);
        join_ring(node, existing_ip, existing_port);
    } else {
        create_ring(node);
    }

    pthread_t node_tid;
    if (pthread_create(&node_tid, nullptr, node_thread, node) != 0) {
        perror("pthread_create for node");
        return EXIT_FAILURE;
    }

    std::cout << "Node running at " << ip << ":" << port << std::endl;

    handle_user_commands(node);

    std::cout << "Exiting..." << std::endl;

    pthread_cancel(listener_tid);
    pthread_cancel(node_tid);
    pthread_join(listener_tid, nullptr);
    pthread_join(node_tid, nullptr);
    cleanup_node(node);

    return 0;
}
