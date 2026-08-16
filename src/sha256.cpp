#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <signal.h>
#include <openssl/sha.h>
#include <iomanip>
#include <unordered_map>
#include <set>
#include <chrono>
#include <algorithm>
#include <openssl/evp.h>
#include <fcntl.h>

#include "sha256.h"


using namespace std;

string computeHash(string& filePath) {
    int fd = open(filePath.c_str(), O_RDONLY);
    if (fd < 0) {
        cerr << "Error opening file: " << strerror(errno) << endl;
        return "";
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        cerr << "Error initializing EVP context" << endl;
        close(fd);
        return "";
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        cerr << "Error initializing SHA-256 digest" << endl;
        EVP_MD_CTX_free(ctx);
        close(fd);
        return "";
    }

    char buffer[CHUNK_SIZE];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        if (EVP_DigestUpdate(ctx, buffer, bytesRead) != 1) {
            cerr << "Error updating SHA-256 digest" << endl;
            EVP_MD_CTX_free(ctx);
            close(fd);
            return "";
        }
    }

    if (bytesRead < 0) {
        cerr << "Error reading file: " << strerror(errno) << endl;
        EVP_MD_CTX_free(ctx);
        close(fd);
        return "";
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLength = 0;
    if (EVP_DigestFinal_ex(ctx, hash, &hashLength) != 1) {
        cerr << "Error finalizing SHA-256 digest" << endl;
        EVP_MD_CTX_free(ctx);
        close(fd);
        return "";
    }

    EVP_MD_CTX_free(ctx);
 
    close(fd);
    
    stringstream ss;
    for (unsigned int i = 0; i < hashLength; i++) {
        ss << hex << setw(2) << setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}
string shapiece(const string& filename, int piece_number) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
        cerr << "Error opening file: " << strerror(errno) << endl;
        return "";
    }

  
    off_t offset = piece_number * PIECE_SIZE;
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        cerr << "Error seeking to piece position: " << strerror(errno) << endl;
        close(fd);
        return "";
    }

    vector<char> buffer(PIECE_SIZE);
    ssize_t bytes_read = read(fd, buffer.data(), PIECE_SIZE);
    if (bytes_read < 0) {
        cerr << "Error reading file: " << strerror(errno) << endl;
        close(fd);
        return "";
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        cerr << "Error initializing EVP context" << endl;
        close(fd);
        return "";
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, buffer.data(), bytes_read) != 1) {
        cerr << "Error computing SHA-256 hash" << endl;
        EVP_MD_CTX_free(ctx);
        close(fd);
        return "";
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_length = 0;
    if (EVP_DigestFinal_ex(ctx, hash, &hash_length) != 1) {
        cerr << "Error finalizing hash computation" << endl;
        EVP_MD_CTX_free(ctx);
        close(fd);
        return "";
    }
    EVP_MD_CTX_free(ctx);
    close(fd);
    stringstream ss;
    for (unsigned int i = 0; i < hash_length; ++i) {
        ss << hex << setw(2) << setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}