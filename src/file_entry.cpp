#include "file_entry.hpp"
#include "protocol.hpp"
#include "stabilization.hpp"
#include "threads.hpp"
#include "utils.hpp"
#include "sha1.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <dirent.h>
#include <vector>

char outdir[MAX_FILEPATH] = "./";

int set_outdir(const char *new_outdir) {
    DIR *dir = opendir(new_outdir);
    if (strlen(new_outdir) >= sizeof(outdir) || dir == nullptr) {
        if (dir) closedir(dir);
        return -1;
    }
    closedir(dir);
    strcpy(outdir, new_outdir);
    return 0;
}

int store_file(Node *n, const char *filepath) {
    if (access(filepath, F_OK) < 0) {
        return -1;
    }

    const char *filename = strrchr(filepath, '/');
    filename = (filename == nullptr) ? filepath : filename + 1;

    uint8_t file_id[HASH_SIZE];
    hash(filename, file_id);

    const Node *responsible_node = find_successor(n, file_id);

    if (memcmp(n->id, responsible_node->id, HASH_SIZE) == 0) {
        delete responsible_node;
        return internal_store_file(n, filename, filepath, file_id, n->ip, n->port);
    }

    Message msg;
    msg.request_id = generate_id();
    strcpy(msg.type, MSG_STORE_FILE);
    memcpy(msg.id, file_id, HASH_SIZE);
    strcpy(msg.ip, n->ip);
    msg.port = n->port;
    strncpy(msg.data, filepath, sizeof(msg.data) - 1);
    msg.data[sizeof(msg.data) - 1] = '\0';

    int result = send_message(n, responsible_node->ip, responsible_node->port, &msg);
    delete responsible_node;
    return result;
}

void confirm_file_stored(Node *node, const char *filepath) {
    FileEntry *file_entry = new FileEntry;

    strncpy(file_entry->filepath, filepath, sizeof(file_entry->filepath) - 1);
    file_entry->filepath[sizeof(file_entry->filepath) - 1] = '\0';
    
    const char *filename = strrchr(filepath, '/');
    filename = (filename == nullptr) ? filepath : filename + 1;

    hash(filename, file_entry->id);
    strncpy(file_entry->filename, filename, sizeof(file_entry->filename) - 1);
    file_entry->filename[sizeof(file_entry->filename) - 1] = '\0';
    strcpy(file_entry->owner_ip, node->ip);
    file_entry->owner_port = node->port;

    file_entry->next = node->uploaded_files;
    node->uploaded_files = file_entry;
}

int internal_store_file(Node *n, const char *filename, const char *filepath, const uint8_t *file_id,
                        const char *uploader_ip, const int uploader_port) {
    FileEntry *new_entry = new FileEntry;

    memcpy(new_entry->id, file_id, HASH_SIZE);
    strncpy(new_entry->filename, filename, sizeof(new_entry->filename) - 1);
    new_entry->filename[sizeof(new_entry->filename) - 1] = '\0';
    strncpy(new_entry->filepath, filepath, sizeof(new_entry->filepath) - 1);
    new_entry->filepath[sizeof(new_entry->filepath) - 1] = '\0';
    strcpy(new_entry->owner_ip, uploader_ip);
    new_entry->owner_port = uploader_port;

    new_entry->next = n->files;
    n->files = new_entry;

    return 0;
}

FileEntry *find_file(Node *n, const char *filename) {
    uint8_t file_id[HASH_SIZE];
    hash(filename, file_id);

    Node *responsible_node = find_successor(n, file_id);

    if (memcmp(n->id, responsible_node->id, HASH_SIZE) == 0) {
        delete responsible_node;
        FileEntry *current = n->files;
        while (current != nullptr) {
            if (memcmp(current->id, file_id, HASH_SIZE) == 0) {
                FileEntry *found = new FileEntry; // Create a copy
                memcpy(found, current, sizeof(FileEntry));
                found->next = nullptr;
                return found;
            }
            current = current->next;
        }
        return nullptr;
    }

    Message msg;
    msg.request_id = generate_id();
    strcpy(msg.type, MSG_FIND_FILE);
    memcpy(msg.id, file_id, HASH_SIZE);
    strcpy(msg.ip, n->ip);
    msg.port = n->port;
    strncpy(msg.data, filename, sizeof(msg.data) - 1);
    msg.data[sizeof(msg.data)-1] = '\0';

    send_message(n, responsible_node->ip, responsible_node->port, &msg);
    delete responsible_node;

    const Message *response = pop_message(&reply_queue, msg.request_id);
    if (response == nullptr || strcmp(response->data, "File not found") == 0) {
        if(response) delete response;
        return nullptr;
    }

    FileEntry *file_entry = new FileEntry;
    memcpy(file_entry->id, response->id, HASH_SIZE);
    strncpy(file_entry->filename, filename, sizeof(file_entry->filename) - 1);
    file_entry->filename[sizeof(file_entry->filename) - 1] = '\0';
    strcpy(file_entry->owner_ip, response->ip);
    file_entry->owner_port = response->port;
    // ... handle multi-segment filepath from response
    strncpy(file_entry->filepath, response->data, sizeof(file_entry->filepath)-1);
    file_entry->filepath[sizeof(file_entry->filepath)-1] = '\0';
    
    delete response;
    return file_entry;
}

FileEntry *find_uploaded_file(const Node *n, const char *filepath) {
    FileEntry *current = n->uploaded_files;
    while (current != nullptr) {
        if (strcmp(current->filepath, filepath) == 0) {
            return current;
        }
        current = current->next;
    }
    return nullptr;
}

int download_file(const Node *n, const FileEntry *file_entry) {
    // ... Implementation would be similar, using C++ file streams (fstream)
    // and ensuring proper memory management for buffers.
    return 0; // Placeholder
}

int delete_file_entry(FileEntry **pFiles, const uint8_t *id) {
    FileEntry *temp = *pFiles;
    FileEntry *prev = nullptr;

    if (temp != nullptr && memcmp(temp->id, id, HASH_SIZE) == 0) {
        *pFiles = temp->next;
        delete temp;
        return 0;
    }

    while (temp != nullptr && memcmp(temp->id, id, HASH_SIZE) != 0) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == nullptr) return -1;

    if (prev) prev->next = temp->next;
    delete temp;
    return 0;
}

int delete_file(Node *n, const char *filename) {
    // ... Similar logic to the C version, but using new/delete
    // and other C++ features.
    return 0; // Placeholder
}
