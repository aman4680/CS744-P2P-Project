#include "threads.hpp"
#include "node.hpp"
#include "stabilization.hpp"
#include "file_entry.hpp"

#include <cctype>
#include <pthread.h>
#include <csignal>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

MessageQueue reply_queue;
MessageQueue download_queue;

void *node_thread(void *arg) {
    Node *n = static_cast<Node *>(arg);
    int next_finger_to_fix = 0;
    while (true) {
        stabilize(n);
        fix_fingers(n, &next_finger_to_fix);
        check_predecessor(n);
        sleep(15);
    }
    return nullptr;
}

void *listener_thread(void *arg) {
    Node *node = static_cast<Node *>(arg);
    Message msg;

    while (true) {
        if (receive_message(node, &msg) > 0) {
            handle_requests(node, &msg);
        }
    }
    return nullptr;
}

// main thread (blocking)
void handle_user_commands(Node *node) {
    char command[4096];
    while (true) {
        if (fgets(command, sizeof(command), stdin) == nullptr) break;
        command[strcspn(command, "\n")] = '\0'; // remove trailing newline

        if (strncmp(command, "outdir", 6) == 0) {
            char new_outdir[512] = {0};
            sscanf(command, "outdir %511s", new_outdir);
            if (strlen(new_outdir) == 0 || new_outdir[strlen(new_outdir) - 1] != '/') {
                std::cout << "Invalid directory" << std::endl;
                continue;
            }
            if (set_outdir(new_outdir) < 0) {
                std::cout << "Failed to set output directory" << std::endl;
            } else {
                std::cout << "Output directory set to " << new_outdir << std::endl;
            }
        } else if (strncmp(command, "store", 5) == 0) {
            char filepath[512] = {0};
            sscanf(command, "store %511s", filepath);
            if (strlen(filepath) == 0) {
                std::cout << "Invalid filepath" << std::endl;
                continue;
            }
            if (store_file(node, filepath) < 0) {
                std::cout << "Failed to store file" << std::endl;
                continue;
            }
            confirm_file_stored(node, filepath);
            std::cout << "File stored successfully" << std::endl;
        } else if (strncmp(command, "find", 4) == 0) {
            char filename[256] = {0};
            sscanf(command, "find %255s", filename);
            if (strlen(filename) == 0) {
                std::cout << "Invalid filename" << std::endl;
                continue;
            }
            FileEntry *file = find_file(node, filename);
            if (file == nullptr) {
                std::cout << "File '" << filename << "' not found" << std::endl;
            } else {
                std::cout << "File '" << filename << "' found at " << file->owner_ip << ":" << file->owner_port << std::endl;
                std::cout << "Do you want to download this file? [y/n] ";
                const char c = getchar();
                getchar(); // consume newline

                if (toupper(c) == 'Y') {
                    std::cout << "Starting download..." << std::endl;
                    if (download_file(node, file) < 0) {
                        std::cout << "Failed to download file" << std::endl;
                    } else {
                        std::cout << "File downloaded successfully" << std::endl;
                    }
                } else {
                    std::cout << "Download canceled." << std::endl;
                }
                delete file; // free the allocated FileEntry
            }
        } else if (strcmp(command, "uploaded") == 0) {
            FileEntry *cur = node->uploaded_files;
            if (cur == nullptr) {
                std::cout << "No files uploaded yet." << std::endl;
            } else {
                std::cout << "Files uploaded:" << std::endl;
                while (cur != nullptr) {
                    std::cout << "  " << cur->filename << std::endl;
                    cur = cur->next;
                }
            }
        } else if (strncmp(command, "delete", 6) == 0) {
            char filename[256] = {0};
            sscanf(command, "delete %255s", filename);
            if (strlen(filename) == 0) {
                std::cout << "Invalid filename" << std::endl;
                continue;
            }
            if (delete_file(node, filename) < 0) {
                std::cout << "Failed to delete file" << std::endl;
            } else {
                std::cout << "File deleted successfully" << std::endl;
            }
        } else if (strcmp(command, "help") == 0 || strcmp(command, "?") == 0) {
            std::cout << "Available commands:\n"
                      << "  outdir <directory> - set the output directory for downloaded files\n"
                      << "  store <filepath> - store a file in the network\n"
                      << "  find <filename> - find a file in the network\n"
                      << "  uploaded - list all files uploaded by the user\n"
                      << "  delete <filename> - delete a file uploaded by the user\n"
                      << "  help/? - show this help message\n"
                      << "  exit - exit the program" << std::endl;
        } else if (strcmp(command, "exit") == 0) {
            std::cout << "Do you really want to quit? [y/n] ";
            const char c = getchar();
            if (toupper(c) == 'Y') {
                break;
            }
            getchar(); // Consume newline
        } else {
            std::cout << "Invalid command." << std::endl;
        }
    }
}

void handle_exit(int sig) {
    signal(sig, SIG_IGN);
    std::cout << "\nDo you really want to quit? [y/n] ";
    const char c = getchar();
    if (toupper(c) == 'Y') {
        std::cout << "Exiting..." << std::endl;
        exit(0);
    }
    signal(SIGINT, handle_exit);
    getchar();
}
