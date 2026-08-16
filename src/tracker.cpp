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


#include "helper_tracker.h"

using namespace std;

bool running = true;

void peer_input()
{   
    string s;
    getline(cin,s);
   
    if(s=="quit") {
        running=false;
    }

}

int main(int argc, char const *argv[])
{
    if(argc!=3) {
        cerr<<"incorrect amount of arguments <trackerip> <trackerport>";
        exit(1);
    }
    
    string ip=argv[1];

    int port=atoi(argv[2]);
    
    int server_fd;
    struct sockaddr_in server_addr;
    if((server_fd=socket(AF_INET,SOCK_STREAM,0))==0)
    {
        cerr<<"error in creating socket";
        exit(0);
    }
    int opt = 1;
if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
{
    cerr << "setsockopt failed";
    exit(1);
}

    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(port);
    if(inet_pton(AF_INET,ip.c_str(),&server_addr.sin_addr)<=0)
    {
        cerr<<"error in creating socket";
        exit(1);
    }
    if(bind(server_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0)
    {
        cerr<<"bind failed";
        exit(1);
    }
    if(listen(server_fd,3)<0)
    {
        cerr<<"listen failed";
        exit(1);
    }
    cout<<"Tracker Server listening on : "<<ip<<" "<<port<<endl;    
    fcntl(server_fd, F_SETFL, O_NONBLOCK); 
    thread input_thread(peer_input);
    while(running) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket >= 0) {
            cout << "New client connected." << endl;
            thread clientThread(handleClient, client_socket);
            clientThread.detach();
        }
    }
    input_thread.join();
    close(server_fd);
    return 0;
}