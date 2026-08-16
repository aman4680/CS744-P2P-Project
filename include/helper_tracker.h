#ifndef funcs
#define funcs

#include<string>
#include<vector>

using namespace std;

void handleClient(int);
int create_peer(vector<string>);
int login(vector<string> ,int );
int create_group(vector<string> ,int);
int join_group(vector<string> ,int);
int list_requests(vector<string> ,int);
void show_file_pieces();
int accept_request(vector<string>,int);
int leave_group(vector<string> ,int );
int list_groups(vector<string> , int );
int upload_file(vector<string> ,int );
int download_file(vector<string> ,int ,string &);
int list_files(vector<string> , int );
int logout(vector<string> ,int );
int stop_share(vector<string> ,int );
int show_downloads(vector<string> ,int );


#endif