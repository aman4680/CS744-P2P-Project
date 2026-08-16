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
#include <unordered_map>
#include <set>
#include <algorithm>

#include "helper_peer.h"
#include "sha256.h"

using namespace std;

void display_piece_map(const unordered_map<int, set<piece_info>>& piece_map) {
    for (const auto& entry : piece_map) {
        int piece_number = entry.first; 
        const set<piece_info>& pieces = entry.second; 
        
        cout << "Piece Number: " << piece_number << endl;
        for (const auto& piece : pieces) {
            cout << "  Client Port: " << piece.client_port 
                 << ", Client IP: " << piece.client_ip 
                 << ", File Path: " << piece.file_path 
                 << ", Hash: " << piece.hash << endl;
        }
    }
}

string recv_string(int sock)
{
    uint32_t length;
     if (recv(sock, &length, sizeof(length), 0) <= 0) {
        return "";
    }
    length = ntohl(length);
    
        vector<char> buffer(length);
    ssize_t received = 0;
    while (received < length) {
        ssize_t result = recv(sock, buffer.data() + received, length - received, 0);
        if (result <= 0) {
            return "";
        }
        received += result;
    }
    
    return string(buffer.begin(), buffer.end());
     
}

void send_string(int sock, const string& data) {
    uint32_t length = htonl(data.size());
    send(sock, &length, sizeof(length), 0);
    send(sock, data.c_str(), data.size(), 0);
}

void handle_peer(int client_socket)
{    string path = recv_string(client_socket);
    if (path.empty()) {
        cerr << "Error receiving file path\n";
        close(client_socket);
        return;
    }

    string piece = recv_string(client_socket);
    if (piece.empty()) {
        cerr << "Error receiving piece number\n";
        close(client_socket);
        return;
    }

    int piece_number=stoi(piece);   

    int fd=open(path.c_str(),O_RDONLY);
    if(fd<0)
    {
        cerr<<"error in opening file";
        close(client_socket);
        exit(1);
    }
    char file_chunk[CHUNK_SIZE];
    ssize_t bytesread;
     off_t offset=piece_number*PIECE_SIZE;
    if(lseek(fd,offset,SEEK_SET)==(off_t)-1)
    {
        cerr<<"error in downloading...\n";
        close(client_socket);
        close(fd);
        return;
    }
    ssize_t total_bytes_read = 0;  

    while (total_bytes_read < PIECE_SIZE) {
        
        ssize_t bytes_to_read = min(static_cast<ssize_t>(sizeof(file_chunk)), PIECE_SIZE - total_bytes_read);

        bytesread = read(fd, file_chunk, bytes_to_read);
        if (bytesread < 0) {
            cerr << "Error reading from file: " << path << endl;
            break;
        }
        if (bytesread == 0) {
            break; 
        }
        ssize_t sentbytes = 0;
        ssize_t totalsent = 0;

        while (totalsent < bytesread) {
            sentbytes = send(client_socket, file_chunk + totalsent, bytesread - totalsent, 0);
            if (sentbytes < 0) {
                cerr << "Error sending file chunk" << endl;
                close(client_socket);
                close(fd);
                return;
            }
            totalsent += sentbytes;
        }

        total_bytes_read += bytesread;
    }
    close(fd);
    close(client_socket);
   
 }

void send_file(int port,string ip)
{
    int send_fd;
    struct sockaddr_in send_addr;
    if((send_fd=socket(AF_INET,SOCK_STREAM,0))==0)
    {
        cerr<<"error in creating socket";
        exit(1);
    }
    int opt=1;
    if(setsockopt(send_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))<0)
    {
        cerr<<"setockopt failed";
        exit(1);
    }
    send_addr.sin_family=AF_INET;
    send_addr.sin_port=htons(port);
    if(inet_pton(AF_INET,ip.c_str(),&send_addr.sin_addr)<=0)
    {
        cerr<<"error in creating socket";
        exit(1);
    }
    if(bind(send_fd,(struct sockaddr*)&send_addr,sizeof(send_addr))<0)
    {
        cerr<<"bind failed";
        exit(1);
    }
    if(listen(send_fd,3)<0)
    {
        cerr<<"listen failed";
        exit(1);
    }
    fcntl(send_fd,F_SETFL,O_NONBLOCK);
    while(true)
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len=sizeof(client_addr);
        int client_socket=accept(send_fd,(struct sockaddr*)&client_addr,&client_addr_len);
        if(client_socket>=0)
        {
            thread new_peer(handle_peer,client_socket);
            new_peer.detach();
        }
    }
    close(send_fd);
}

void download(int piece_number,int port, string ip, string path, string hash, string dstn,int sock_fd,string group_name) {
   
    int retries = 0;
    bool success = false;
    
    while (retries < MAX_RETRIES && !success) {
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        cerr << "Error converting IP address" << endl;
        return;
    }

    int download_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (download_sock < 0) {
        cerr << "Error creating socket: " << strerror(errno) << endl;
        return;
    }

    if (connect(download_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Error connecting to peer: " << strerror(errno) << endl;
        close(download_sock);
        return;
    }

   
    send_string(download_sock,path);
    send_string(download_sock,to_string(piece_number));
    char file_chunk[CHUNK_SIZE];
 
    int file_fd = open(dstn.c_str(), O_WRONLY | O_CREAT , 0644);
    if (file_fd < 0) {
      
        close(download_sock);
        retries++;
        this_thread::sleep_for(chrono::milliseconds(RETRY_DELAY_MS));
        continue;;
    }
   off_t offset = static_cast<off_t>(piece_number) * PIECE_SIZE;
    if (lseek(file_fd, offset, SEEK_SET) == (off_t)-1) {
        cerr << "Error seeking in file: " << strerror(errno) << endl;
        close(download_sock);
        close(file_fd);
        return;
    }
   ssize_t total_bytes_received = 0;
    while (total_bytes_received < PIECE_SIZE) {
        ssize_t bytes_received = recv(download_sock, file_chunk, sizeof(file_chunk), 0);
        if (bytes_received < 0) {
           
            break;
        }
        if (bytes_received == 0) {
            break;
        }

        ssize_t written_bytes = write(file_fd, file_chunk, bytes_received);
        if (written_bytes < 0) {
            cerr << "Error writing to file: " << strerror(errno) << endl;
            break;
        }
        total_bytes_received += bytes_received;
    }


    
    if (total_bytes_received == PIECE_SIZE && (shapiece(path,piece_number)==hash)) {
            success = true;
            cout<<"port"<<port<<"ip"<<ip<<"piece number"<<piece_number<<endl;
           
        } else {
            retries++;
            this_thread::sleep_for(chrono::milliseconds(RETRY_DELAY_MS));
        }
    close(file_fd);
    close(download_sock);
    }
    
}

void piece_creation(int sock_fd,string file_path,string s)
{   
    cout<<"now seeding\n";
    int fd=open(file_path.c_str(),O_RDONLY);
    int i=0;
    char buffer1[BUFFER_SIZE];
    memset(buffer1,0,sizeof(buffer1));
    int bytesReceived;
    ssize_t bytesread;
 
    char buffer[PIECE_SIZE];
    while((bytesread=read(fd,buffer,sizeof(buffer)))>0){
    string hash=shapiece(file_path,i);
    string message = s+" "+to_string(i)+" "+hash;
    send(sock_fd,message.c_str(),message.size(),0);
 
    bytesReceived=recv(sock_fd,buffer1,sizeof(buffer1)-1,0);
    i++;
    }  
    
    close(fd);
}
