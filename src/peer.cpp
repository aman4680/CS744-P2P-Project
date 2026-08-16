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


#include "helper_peer.h"
#include "sha256.h"

int main(int argc, char const *argv[]){
  if(argc!=4)
    {
        cerr<<"incorrect amount of arguments Usage <peerip:port> <trackerip> <trackerport>";
        exit(1);
    }
    string ip=argv[1];
    string ip_addr=ip.substr(0,ip.find(':'));
    int port = stoi(ip.substr(ip.find(':')+1,ip.length()));
   
    char buffer[256];
    int bytesRead;
    
    string serverIPAddress=argv[2];
    int serverPortNumber=atoi(argv[3]);

    int sock_fd=socket(AF_INET,SOCK_STREAM,0);
    thread send_data(send_file,port,ip_addr);
    send_data.detach();
    sockaddr_in server_addr;
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(serverPortNumber);
    if(inet_pton(AF_INET,serverIPAddress.c_str(),&server_addr.sin_addr)<=0)
    {
        cerr<<"error in creating socket";
        exit(1);
    }
    if(connect(sock_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0)
    { 
            cerr<<"connection to tracker failed";
            exit(1);
    }
    send(sock_fd, ip.c_str(), ip.size(), 0);
    while (true) {

        string s,dstn;
        cout << "Enter message to send to server: ";
        getline(cin, s);
        string group_name;
        string b = s.substr(0,s.find(" "));
        if(b=="upload_file")
        {
            string file_path=s.substr(s.find(" ")+1,s.find(" ",s.find(" ")+1)-(s.find(" ")+1));
            if(access(file_path.c_str(),F_OK)!=0)
            {
                cout<<"file does not exist\n";
                continue;
            }
            
            int fd = open(file_path.c_str(),O_RDONLY);
            string hash_file=computeHash(file_path);
            int filelen=lseek(fd,0,SEEK_END);
            close(fd);
            string upload_msg="file_details "+file_path+" "+to_string(filelen)+" "+hash_file;
            send(sock_fd,upload_msg.c_str(),upload_msg.size(),0);
            piece_creation(sock_fd,file_path,s);
            continue;
            
        }
        else if(b=="download_file") {      
            cout<<s<<endl;
            ssize_t last=s.find_last_of(" ");
            ssize_t secondlast=s.find_last_of(" ",last-1);
            dstn=s.substr(last+1)+s.substr(secondlast+1,last-secondlast-1);
            cout<<dstn<<endl;
            ssize_t first_space = s.find(" "); 
            if (first_space == static_cast<ssize_t>(string::npos)) {
                group_name=" ";
            }

            ssize_t second_space = s.find(" ", first_space + 1); 
            if (second_space == static_cast<ssize_t>(string::npos)) {
                second_space = s.length(); 
            }
            group_name = s.substr(first_space + 1, second_space - first_space - 1);
            
        }
        else if(b=="help") {
            cout << "Create peer Account: create_peer <peer_id> <password>" <<endl;
            cout << "Login: login <peer_id> <password>" <<endl;
            cout << "Create Group: create_group <group_id>" <<endl;
            cout << "Join Group: join_group <group_id>" <<endl;
            cout << "Leave Group: leave_group <group_id>" <<endl;
            cout << "List Pending Join: list_requests <group_id>" <<endl;
            cout << "Accept Group Joining Request: accept_request <group_id> <peer_id>" <<endl;
            cout << "List All Groups in Network: list_groups" <<endl;
            cout << "List All Sharable Files in Group: list_files <group_id>" <<endl;
            cout << "Upload File: upload_file <file_path> <group_id>" <<endl;
            cout << "Download File: download_file <group_id> <file_name> <destination_path>" <<endl;
            cout << "Logout: logout" <<endl;
            cout << "Show Downloads: show_downloads" <<endl;
            cout << "Output format: [D] [grp_id] filename or [C] [grp_id] filename (D - Downloading, C - Complete)" <<endl;
            cout << "Stop Sharing: stop_share <group_id> <file_name>" <<endl;
            cout <<"Quit: quit"<<endl;
        }
        else if(b=="quit"){
            break;
        }
        if (s.empty())
            continue;
        send(sock_fd, s.c_str(), s.size(), 0);
        char buffer[893228];
        memset(buffer,0,sizeof(buffer));
        int bytesReceived;
        bytesReceived=recv(sock_fd,buffer,sizeof(buffer)-1,0);
        if(bytesReceived>0)
           { 
            string msg=buffer;
           
            string first_word=msg.substr(0,msg.find("\n"));
            buffer[bytesReceived]='\0';
            if(first_word=="download")
                {       
                    
                int source_port;
                int file_size;
                string file_hash;
                string source_ip,source_hash,source_path; 
                ssize_t start=msg.find("\n")+1;
                ssize_t end=msg.find("\n",start);
                file_size=stoi(msg.substr(start,end-start));
                start = end+1;
                end=msg.find("\n",start);
                file_hash=msg.substr(start,end-start);
        
                int linenumber=0;
                start = end+1;
                end=msg.find("\n",start);  
                start = end+1;
                end=msg.find("\n",start); 
                int fd=open(dstn.c_str(),O_WRONLY | O_CREAT ,0666);
                lseek(fd,file_size-1,SEEK_SET);
                write(fd,"",1);
                close(fd);
                int i=0;
                string line1=msg.substr(start,end-start);
               
                unordered_map<int,set<piece_info>> piece_map;
                while(end!=string::npos)
                {   
                    
                    string line=msg.substr(start,end-start);
                  
                    
                    if(line==to_string(i+1))
                        {i=i+1;
                        start=end+1;
                    end=msg.find("\n",start);
                        continue;
                         }
                    
                    if(linenumber%4==0)
                    {   
                        
                        source_port=stoi(line);
                    }
                    else if(linenumber%4==1)
                    {
                        source_ip=line;
                    }
                    else if(linenumber%4==2)
                    {
                        source_path=line;
                    }
                    else
                    {   
                        source_hash=line;
                        piece_info p(source_port,source_ip,source_path,source_hash);
                        piece_map[i].insert(p);
                    }
                    start=end+1;
                    end=msg.find("\n",start);
                    linenumber++; 
                }
             
                vector<pair<int,int>> piece_rarity;
                for(const auto& pair:piece_map)
                {
                    int piece_number=pair.first;
                    int piece_count=pair.second.size();
                    piece_rarity.push_back({piece_count,piece_number});
                }
                sort(piece_rarity.begin(),piece_rarity.end());
                for(const auto& rarity_pair:piece_rarity)
                {
                    int piece_number=rarity_pair.second;
                    const set<piece_info>& pieces = piece_map[piece_number];
                    auto it = next(pieces.begin(), rand() % pieces.size());
                    const piece_info& selected_peer = *it;
                    download(piece_number,selected_peer.client_port,selected_peer.client_ip,selected_peer.file_path,selected_peer.hash,dstn,sock_fd,group_name);
                }
      
                if(file_hash==computeHash(dstn))
                {
                    cout<<"download completed\n";
                }
                else
                {
                    cout<<"download failed\n";
                    unlink(dstn.c_str());
                }
                 string message="upload_file "+dstn+" "+group_name;
                 piece_creation(sock_fd,dstn,message);
                 size_t pos = dstn.find_last_of("/\\");
                 string filename = (pos == string::npos) ? dstn : dstn.substr(pos + 1);
                 string download_completed="download_completed "+group_name+" "+filename;
            
            }   
            else
            {
            cout<<buffer;
            }
    
           }

    }
    close(sock_fd);
    return 0;
}
