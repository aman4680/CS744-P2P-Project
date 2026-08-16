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
#include "utils.h"

using namespace std;
#define BUFFER 1024

unordered_map<string,string> user;
unordered_map<string,bool> loggedIn;
unordered_map<int,string> logmap;
unordered_map<string,string> group;
unordered_map<string,set<string>> group_members;
unordered_map<string,set<string>> pending;
unordered_map<string,set<file_info>> file_list;
unordered_map<string,set<string>> group_file;
unordered_map<string,set<string,string>> file_path_save;
unordered_map<int,pair<int,string>> listenmap;
unordered_map<string,set<pair<string,pair<string,string>>>> downloads;
unordered_map<string, vector<filepiece>> file_pieces;
unordered_map<string,pair<int,string>> filedetails;

int create_peer(vector<string> input)
{  
    if(input.size()!=3)
    {
        return -1;
    }
    if(user.find(input[1])!=user.end())
    {
        return -2;
    }
    user.insert({input[1],input[2]});
    loggedIn.insert({input[1],false});
    return 1;
}
int login(vector<string> input,int clien_socket)
{
    if(input.size()!=3)
    {
        cerr<<"incorrect amount of arguments";
        return 2;
    }
    if(logmap.find(clien_socket)!=logmap.end())
    {
        return 3;
    }
    if(loggedIn[input[1]]==true)
    {   
        return 4;
    }
    if(user.find(input[1])!=user.end())
    {
        if(user[input[1]]==input[2])
        {
            loggedIn[input[1]]=true;
            logmap.insert({clien_socket,input[1]});
            return 1;
        }
        
    }
    return 0;
}
int create_group(vector<string> input,int client_socket)
{   
    if(input.size()!=2)
    {
        return -1;
    }
    if(group.find(input[1])!=group.end())
    {
        return 0;
    }
    string name=logmap[client_socket];
    group.insert({input[1],name});
    group_members[input[1]].insert(name);
    return 1;
}
int join_group(vector<string> input,int client_socket)
{   
    if(input.size()!=2)
    {
        return -1;
    }
    string group_name=input[1];
    string name=logmap[client_socket];
    if(group.find(input[1])==group.end())
    {
        return -2;
    }
    if(group_members[group_name].count(name)>0)
    {
        return 0;
    }
    pending[group_name].insert(name);
    return 1;
}
int list_requests(vector<string> input,int client_socket)
{
    if(input.size()!=2)
    {
        return -1;
    }
    string group_name=input[1];
    if(group.find(group_name)==group.end())
        {
            return -2;
        }
    if(logmap[client_socket]==group[group_name])
    {   
        if(pending[group_name].size()>0){
        for(auto it : pending[group_name])
        {   
            string message = it+"\n";
            send(client_socket,message.c_str(),message.size(),0);
        }
        }
        else
        {
            string message = "no pending request\n";
            send(client_socket,message.c_str(),message.size(),0);
        }
        return 1;
    }
   
    return 0;
}

void show_file_pieces()
{
    for (const auto& file : file_pieces) 
    {
        cout << "File Name: " << file.first << endl;
        const vector<filepiece>& pieces = file.second;
        for (const auto& piece : pieces) 
        {
            cout << "  Piece Number: " << piece.piecenumber << endl;
            for (const auto& source : piece.sources) 
            {
                cout << "    Source Info:" << endl;
                cout << "      peername: " << source.peername << endl;
                cout << "      Client Port: " << source.client_port << endl;
                cout << "      Client IP: " << source.client_ip << endl;
                cout << "      File Path: " << source.file_path << endl;
                cout << "      Hash: " << source.hash << endl;
            }
        }
        cout << endl; 
    }
}

int accept_request(vector<string> input,int client_socket)
{
    if(input.size()!=3)
    {
        return -1;
    }
    string group_name =input[1];
    string peer_name = input[2];
    string my_name = logmap[client_socket];
    if(group.find(group_name)==group.end())
        {
            return -2;
        }
    if(pending[group_name].find(peer_name)==pending[group_name].end())
    {
        return -3;
    }
        if(logmap[client_socket]==group[group_name])
    {
        group_members[group_name].insert(peer_name);
        pending[group_name].erase(peer_name);
        return 1;
    }
   
    return 0;
}
int leave_group(vector<string> input,int client_socket)
{
    if(input.size()!=2)
    {
        return -1;
    }
    string peer_name=logmap[client_socket];
    string group_name=input[1];
    if(group.find(group_name)==group.end())
      { 
         return -2;
      }
    if(group_members[group_name].find(peer_name)==group_members[group_name].end())
    {
        return -3;
    }
    if(group[group_name]==peer_name)
    {   
        if(group_members[group_name].size()>1)
        {
            for(auto it:group_members[group_name])
            {   if(it!=peer_name){
                group[group_name]=it;
                break;
                }
            }
             group_members[group_name].erase(peer_name);
        }
        else
        {
            group.erase(group_name);
            group_members.erase(group_name);
        }
    }
    else
    {
         group_members[group_name].erase(peer_name);
    }
    return 1;
}
int list_groups(vector<string> input, int client_socket)
{
    if(input.size()!=1)
    {
        return -1;
    }
    if(group.size()>0)
    for(auto it : group)
    {
        string message=it.first+"\n";
        send(client_socket,message.c_str(),message.size(),0);
    }
    else
    {
        string message="no groups present\n";
        send(client_socket,message.c_str(),message.size(),0);
    }
    return 0;
}
int upload_file(vector<string> input,int client_socket)
{      
     if(input.size()!=5)
     {
        cout<<"failed\n";
        return -1;
     }
    string group_name=input[2];
    string peer_name=logmap[client_socket];
    if(group.find(group_name)==group.end())
     {
        return -2;
     }
     if(group_members[group_name].find(peer_name)==group_members[group_name].end())
     {
        return -3;
     }
     int client_port=listenmap[client_socket].first;
     string client_ip=listenmap[client_socket].second;
     string file_path = input[1];
     string hash=input[4];
     int last = file_path.find_last_of("/\\");
     string file_name;
     if(last==string::npos)
     {
        file_name=file_path;
     }
     else
     {
        file_name=file_path.substr(last+1);
     }
     int piece_number=stoi(input[3]);
    
     file_info finfo(peer_name,client_port,client_ip,file_path,hash);

    filepiece piece(file_name,piece_number);
    piece.sources.insert(finfo);
    if(file_pieces.find(file_name)==file_pieces.end())
    {
         file_pieces[file_name] = vector<filepiece>();
    }
    bool piecefound=false;
    for(int i=0;i<file_pieces[file_name].size();i++)
    {
        if(file_pieces[file_name][i].piecenumber==piece_number)
        {   
            file_pieces[file_name][i].sources.insert(finfo);
            piecefound=true;
            break;
        }
    }
    if(!piecefound)
    {   
        filepiece piecea(file_name,piece_number);
        piecea.sources.insert(finfo);
        file_pieces[file_name].push_back(piecea);
    }
    show_file_pieces();
    file_list[file_name].insert(finfo);
    group_file[group_name].insert(file_name);
    return 0;
}
int download_file(vector<string> input,int client_socket,string &peerdetails)
{
    if(input.size()!=4)
    {
        return -1;
    }
    string group_name=input[1];
    string file_name=input[2];
    string destn=input[3];
    string peer=logmap[client_socket];
    cout<<endl<<group_name<<" "<<file_name<<" "<<destn<<" "<<peer;
    if(group.find(group_name)==group.end())
    {
        return -2;
    }

    if(group_members[group_name].find(peer)==group_members[group_name].end())
    {
        return -4;
    }
    if(group_file[group_name].find(file_name)==group_file[group_name].end())
    {
        return -3;
    }
    
    downloads[peer].insert({file_name,{"C",group_name}});
    peerdetails="download\n";
    peerdetails+=to_string(filedetails[file_name].first)+"\n";
    peerdetails+=filedetails[file_name].second+"\n";
    for(auto i:file_pieces[file_name])
    {
    peerdetails+=to_string(i.piecenumber)+"\n";
    for(auto it : i.sources)
    {
        peerdetails+=to_string(it.client_port)+"\n";
        peerdetails+=it.client_ip+"\n";
        peerdetails+=it.file_path+"\n";
        peerdetails+=it.hash+"\n";
    }
    }

    return 0;
}
int list_files(vector<string> input, int client_socket)
{
    if (input.size() != 2)
    {
        return -1; 
    }
    string group_name = input[1];
    if (group.find(group_name) == group.end())
    {
        return -2; 
    }
    if (group_file.find(group_name) == group_file.end() || group_file[group_name].empty())
    {
        return -3; 
    }
    for (const auto &file_name : group_file[group_name])
    {
        string message = file_name + "\n";
        send(client_socket, message.c_str(), message.size(), 0);
    }
    return 0; 
}

int logout(vector<string> input,int client_socket)
{
    if(input.size()!=1)
    {
        return -1;
    }
    string peername=logmap[client_socket];
    loggedIn[peername]=false;
    logmap.erase(client_socket);
    return 0;
}
int stop_share(vector<string> input,int client_socket)
{
    if(input.size()!=3)
    {
        return -1;
    }
    string group_name=input[1];
    string file_name=input[2];
    string peer_name=logmap[client_socket];
    if(group.find(group_name)==group.end())
    {
        return -2;
    }
    if(group_file[group_name].find(file_name)==group_file[group_name].end())
    {
        return -3;
    }
    
    bool flag=true;
     if (file_pieces.find(file_name) != file_pieces.end())
    {
        for (filepiece &piece : file_pieces[file_name])
        {
            for (auto it = piece.sources.begin(); it != piece.sources.end(); )
            {
                if (it->peername == peer_name)
                {
                    it = piece.sources.erase(it); 
                }
                else
                {
                    ++it; 
                }
            }
             if (!piece.sources.empty())
            {
                flag = false; 
            }
        }
         if (flag)
        {
            group_file[group_name].erase(file_name);
            file_pieces.erase(file_name); 
        }
    }
    return 0;
}
int show_downloads(vector<string> input,int client_socket)
{
    if(input.size()!=1)
    {
        return -1;
    }
    string peer=logmap[client_socket];
    if(downloads.find(peer)==downloads.end())
    {
        return -2;
    }
    for(auto it:downloads[peer])
    {
        string message ="["+ it.second.first+"] "+"["+it.second.second +"] " +it.first+"\n";
        send(client_socket,message.c_str(),message.size(),0);
    }
    return 0;
}
void handleClient(int client_socket) {
    char buffer[BUFFER];
    recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    string ip=buffer;
    string ip_addr=ip.substr(0,ip.find(':'));
    int port = stoi(ip.substr(ip.find(':')+1,ip.length()));
    listenmap[client_socket]={port,ip_addr};
    while (true) {

         vector<string> input;
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            cout << "Client disconnected or error occurred." << endl;
            break;
        }
        buffer[bytesRead]='\0';
       
        char* token = strtok(buffer," ");
        while(token!=NULL)
        {
    
        input.push_back(token);
        token=strtok(nullptr," ");
        }
        if(input[0]=="create_peer")
        {   int a = create_peer(input);
            if(a==1){
            string message = "account created\n";
            send(client_socket,message.c_str(),message.size(),0);
            }
            else if(a==-1)
            {
                string message = "Incorrect amount of arguments\n";
                send(client_socket,message.c_str(),message.size(),0);
            }
            else
            {
                string message = "peer already exists\n";
                send(client_socket,message.c_str(),message.size(),0);
            }
        }
        else if(input[0]=="login")
        {   int l = login(input,client_socket);
            if(l==1)
            {   string message = "Logged in\n";
                send(client_socket,message.c_str(),message.size(),0);
            }
            else if(l==0)
            {   
                string message = "Incorrect peer_id or password\n";
                send(client_socket,message.c_str(),message.size(),0);
            }
            else if(l==3)
            {
                string message = "you are logged in as another peer, please logout first\n";
                send(client_socket,message.c_str(),message.size(),0);
            }
            else if(l==4)
            {
                 string message = "someone else is logged in as "+input[1]+"\n";
                send(client_socket,message.c_str(),message.size(),0);
            }
            else
            {
                string message = "Incorrect amount of arguments\n";
                send(client_socket,message.c_str(),message.size(),0);
            }

        }
        else 
        {
            if(logmap.find(client_socket)!=logmap.end())
            {
               if(input[0]=="create_group")
               {    int a = create_group(input,client_socket); 
                    if(a == 1)
                    {
                        string message = "Group created\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==0)
                    {
                        string message = "Group already exists\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else
                    {
                         string message = "Incorrect amount of arguments\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
               }
               else if(input[0]=="join_group")
               {
                    int a =join_group(input,client_socket);
                    if(a==-2)
                    {
                         string message = "No such group exists\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==0)
                    {
                         string message = "you are already member of this group\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-1)
                    {
                         string message = "Incorrect amount of arguments\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else
                    {
                         string message = "Request sent to join group "+ input[1]+"\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }

               }
               else if(input[0]=="leave_group")
               {
                    int a=leave_group(input,client_socket);
                    if(a==-1)
                    {
                        string message = "Incorrect amount of arguments\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-2)
                    {
                        string message = "No such group exists\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-3)
                    {
                        string message = "You are not a part of this group\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else
                    {
                        string message = "You have exited the group "+input[1]+"\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
               }
               else if(input[0]=="list_requests")
               {
                    int a = list_requests(input,client_socket);
                    if(a==-1)
                    {
                        string message = "Incorrect amount of arguments\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==0)
                    {
                        string message = "Sorry you are not the owner of this group\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-2)
                    {
                        string message = "No such group exists\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
               }
               else if(input[0]=="accept_request")
               {
                    int a = accept_request(input,client_socket);
                    if(a==1)
                    {
                        string message = "added "+input[2]+" to group "+input[1]+"\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-1)
                    {
                        string message = "Incorrect amount of arguments\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-2)
                    {
                         string message = "no such group\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-3)
                    {
                         string message = "no such request to join group\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else
                    {
                         string message = "Sorry you are not the owner of this group\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
               }
               else if(input[0]=="list_groups")
               {
                    int a =list_groups(input,client_socket);
                    if(a==-1)
                    {
                        string message = "Incorrect amount of arguments\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
               }
               else if(input[0]=="upload_file")
               {
                    int a = upload_file(input,client_socket);
                    for(int i=0;i<input.size();i++)
                             cout<<input[i]<<" ";
                    cout<<endl;
                    if(a==-1)
                    {
                        string message = "Incorrect amount of arguments\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-2)
                    {
                        string message = "group does not exist\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-3)
                    {
                        string message = "file is not present in the group\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-4)
                    {
                        string message = "you are not member of the group\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==0)
                    {
                        string message = "message sent for uploading\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                }
                else if(input[0]=="download_file")
                {   
                    string peerdetails="";
                    int a = download_file(input,client_socket,peerdetails);
                    if(a==-1)
                    {
                        string message = "Incorrect amount of arguments\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-2)
                    {
                        string message = "No such group exists\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-3)
                    {
                        string message = "No such file in group\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==0)   
                    {   
                       
                        string message = "No such file in group\n";
                        send(client_socket,peerdetails.c_str(),peerdetails.size(),0);                        
                    }
                }
               else if(input[0]=="list_files")
               {
                int a = list_files(input,client_socket);
                
                    if(a==-1)
                    {
                        string message = "Incorrect amount of arguments\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-2)
                    {
                        string message = "No such group exists\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-3)
                    {
                        string message = "No file in group\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==0)   
                    {   
                                              
                    }
               }
               else if(input[0]=="logout")
               {
               int a = logout(input,client_socket);
               if(a==-1)
               {
                    string message = "Incorrect amount of arguments\n";
                        send(client_socket,message.c_str(),message.size(),0);
               }
               else
               {
                    string message = "logged out\n";
                        send(client_socket,message.c_str(),message.size(),0);
               }
               }
               else if(input[0]=="stop_share")
               {
                    int a=stop_share(input,client_socket);
                    if(a==-1)
                    {
                        string message = "Incorrect amount of arguments\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-2)
                    {
                        string message = "No such group exists\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-3)
                    {
                        string message = "No such file in group\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==0)   
                    {   
                        string message = "stopped sharing file\n";
                        send(client_socket,message.c_str(),message.size(),0);                    
                    }
                    
               }
               else if(input[0]=="show_downloads")
               {    int a = show_downloads(input,client_socket);
                    if(a==-1)
                    {
                        string message = "Incorrect amount of arguments\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else if(a==-2)
                    {
                        string message = "your download history is empty\n";
                        send(client_socket,message.c_str(),message.size(),0);
                    }
                    else
                    {

                    }
               }
               else if(input[0]=="file_details")
               {
                cout<<input[1]<<endl<<input[2]<<endl<<input[3]<<endl;
                string filename;
                size_t pos = input[1].find_last_of("/\\");
                if(pos!=string::npos)
                    filename=input[1].substr(pos+1);
                else 
                    filename=input[1];
                filedetails[filename]={stoi(input[2]),input[3]};
               }
               else if(input[0]=="download_completed")
               {
                string peer=logmap[client_socket];
                cout<<"hi";
                string filename=input[2];
                string groupname=input[1];
                for(auto &it:downloads[peer])
                {   
                    if(it.first==filename)
                        downloads[peer].erase(it);
                    downloads[peer].insert({filename,{"C",groupname}});
                }
               }
               else
               {
                string message = "Unknown Command\n";
                send(client_socket,message.c_str(),message.size(),0);
               }
            }
            else
            {
                string message = "Please login first\n";
                send(client_socket,message.c_str(),message.size(),0);
            }
        }
    }
    close(client_socket);
}