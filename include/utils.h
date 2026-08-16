#ifndef utils
#define utils

#include <unordered_map>
#include <set>
#include <bits/stdc++.h>
#include <string>
#include <iostream>
using namespace std;

#define BUFFER_SIZE 256
#define CHUNK_SIZE 200
#define PIECE_SIZE 200

const int MAX_RETRIES = 3;
const int RETRY_DELAY_MS = 1000;

struct piece_info
{   
    int client_port;
    string client_ip;
    string file_path;
    string hash;

    piece_info(int client_port,string client_ip,string file_path,string hash)
    {   
        this->client_ip=client_ip;
        this->client_port=client_port;
        this->file_path=file_path;
        this->hash=hash;
    }
    
    bool operator<(const piece_info& other) const {
        if (file_path != other.file_path) {
            return file_path < other.file_path;
        }
        if (client_ip != other.client_ip) {
            return client_ip < other.client_ip;
        }
        return client_port < other.client_port;
    }
};

struct file_info
{   
    string peername;
    int client_port;
    string client_ip;
    string file_path;
    string hash;

    file_info(string peername,int client_port,string client_ip,string file_path,string hash)
    {   
        this->peername=peername;
        this->client_ip=client_ip;
        this->client_port=client_port;
        this->file_path=file_path;
        this->hash=hash;
    }
    
    bool operator<(const file_info& other) const {
        if (file_path != other.file_path) {
            return file_path < other.file_path;
        }
        if (client_ip != other.client_ip) {
            return client_ip < other.client_ip;
        }
        return client_port < other.client_port;
    }
};
struct filepiece
{
    string file_name;
    int piecenumber;
    set<file_info> sources; 
    filepiece(string file_name,int piecenumber)
    {
        this->file_name=file_name;
        this->piecenumber=piecenumber;
    }
};

extern unordered_map<string,string> user;
extern unordered_map<string,bool> loggedIn;
extern unordered_map<int,string> logmap;
extern unordered_map<string,string> group;
extern unordered_map<string,set<string>> group_members;
extern unordered_map<string,set<string>> pending;
extern unordered_map<string,set<file_info>> file_list;
extern unordered_map<string,set<string>> group_file;
extern unordered_map<string,set<string,string>> file_path_save;
extern unordered_map<int,pair<int,string>> listenmap;
extern unordered_map<string,set<pair<string,pair<string,string>>>> downloads;
extern unordered_map<string, vector<filepiece>> file_pieces;
extern unordered_map<string,pair<int,string>> filedetails;

#endif
