#include "utils.hpp"
#include "sha1.hpp"
#include "file_entry.hpp" // For FileEntry definition

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <uuid/uuid.h>
#include <vector>

// checks if id is in the interval (a, b)
int is_in_interval(const uint8_t *id, const uint8_t *a, const uint8_t *b) {
    if (memcmp(a, b, HASH_SIZE) < 0) {
        return memcmp(id, a, HASH_SIZE) > 0 && memcmp(id, b, HASH_SIZE) < 0;
    }
    return memcmp(id, a, HASH_SIZE) > 0 || memcmp(id, b, HASH_SIZE) < 0;
}

// generate unique 32 bits id
uint32_t generate_id() {
    uuid_t uuid;
    uuid_generate(uuid);
    return *(reinterpret_cast<uint32_t *>(uuid)); // return the first 32 bits
}

// checks if file_id falls in the interval (new_node_id, current_node_id]
int should_transfer_file(const uint8_t *file_id, const uint8_t *new_node_id, const uint8_t *current_node_id) {
    return memcmp(file_id, new_node_id, HASH_SIZE) > 0 && memcmp(file_id, current_node_id, HASH_SIZE) <= 0;
}

size_t serialize_file_entries(char **buf, size_t buf_size, const FileEntry *files, const uint8_t *new_node_id,
                              const uint8_t *current_node_id) {
    size_t offset = 0;
    const FileEntry *entry = files;

    while (entry != nullptr) {
        if (should_transfer_file(entry->id, new_node_id, current_node_id)) {
            const size_t entry_size = sizeof(FileEntry);
            if (offset + entry_size > buf_size) {
                buf_size *= 2;
                *buf = static_cast<char*>(realloc(*buf, buf_size));
                 if (*buf == nullptr) {
                    perror("realloc");
                    exit(EXIT_FAILURE);
                }
            }

            memcpy(*buf + offset, entry, entry_size);
            offset += entry_size;
        }
        entry = entry->next;
    }

    return offset;
}

size_t serialize_all_file_entries(char **buf, size_t buf_size, const FileEntry *files) {
    size_t offset = 0;
    const FileEntry *entry = files;

    while (entry != nullptr) {
        const size_t entry_size = sizeof(FileEntry);
        if (offset + entry_size > buf_size) {
            buf_size *= 2;
            *buf = static_cast<char*>(realloc(*buf, buf_size));
            if (*buf == nullptr) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
        }

        memcpy(*buf + offset, entry, entry_size);
        offset += entry_size;

        entry = entry->next;
    }

    return offset;
}

void deserialize_file_entries(Node *n, const char *buf, size_t buf_size) {
    size_t offset = 0;

    while (offset < buf_size) {
        // allocate memory for new file entry
        FileEntry *entry = new FileEntry;

        memcpy(entry, buf + offset, sizeof(FileEntry));
        entry->next = nullptr; // make sure the next pointer is NULL

        entry->next = n->files;
        n->files = entry;

        offset += sizeof(FileEntry);
    }
}

void delete_transferred_files(FileEntry **pFiles, const uint8_t *new_node_id, const uint8_t *current_node_id) {
    FileEntry *prev = nullptr;
    FileEntry *entry = *pFiles;

    while (entry != nullptr) {
        if (should_transfer_file(entry->id, new_node_id, current_node_id)) {
            if (prev == nullptr) {
                // removing the head of the file
                *pFiles = entry->next;
            } else {
                prev->next = entry->next;
            }

            FileEntry *to_delete = entry;
            entry = entry->next;
            delete to_delete; // free the memory
        } else {
            prev = entry;
            entry = entry->next;
        }
    }
}

