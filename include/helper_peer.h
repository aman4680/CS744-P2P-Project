#ifndef peer
#define peer

#include <string>
#include <unordered_map>
#include <set>
#include "utils.h"

using namespace std;
void display_piece_map(const unordered_map<int, set<piece_info>>&);
string recv_string(int sock);
void send_string(int sock, const string& data);
void handle_peer(int client_socket);
void send_file(int port,string ip);
void download(int piece_number,int port, string ip, string path, string hash, string dstn,int sock_fd,string group_name);
void piece_creation(int sock_fd,string file_path,string s);

#endif