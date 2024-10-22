#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <arpa/inet.h>
#include <unordered_map>
#include <set>
#include <thread>
#include <sys/stat.h>

using namespace std;
#define SIZE 1024

// user information
unordered_map<string, string> userInfo;
unordered_map<string, bool> isLoggedIn;

// group information
vector<string> groups;
unordered_map<string, string> groupAdmins;
unordered_map<string, set<string>> groupMembers;
unordered_map<string, set<string>> pendingRequests;

// files
unordered_map<string, unordered_map<string, set<string>>> seederList; // <groupid, <file, peer_address>
unordered_map<string, string> fileSize;

unordered_map<string, string> userport;
/*
// for multiple trackers
struct Tracker{
public:
    string tracker_ip;
    string tracker_port;
    bool is_running;
};
*/

// struct User
// {
// public:
//     string userID;
//     string password;
//     bool is_logged_in;

//     string client_ip;
//     string client_port;

//     User(string userID, string password)
//     {
//         userID = userID;
//         password = password;
//         is_logged_in = false;
//     }
// };

// Data structure to store user information
// unordered_map<string, string> userDatabase;
// // Mutex to protect userDatabase
// mutex userDatabaseMutex;

// void register_user(string &userID, string &password)
// {
//     lock_guard<mutex> lock(userDatabaseMutex); // Lock the mutex to access the userDatabase
//     userDatabase[userID] = password;
// }

void error(string err)
{
    cout << err << endl;
}

void fatal(string err)
{
    error(err);
    exit(1);
}

void send_message(int socket, string msg)
{
    write(socket, msg.c_str(), msg.length());
}

void get_tracker_info(string filename, int tracker_no, string &tracker_ip, int &tracker_port)
{
    int file_desc = open(filename.c_str(), O_RDONLY);
    if (file_desc == -1)
        fatal("failed to open file: " + filename);

    // for multiple trackers
    /*
    int current_line = 1;
    bool found = false;

    if (tracker_no == 1)
        found = true;
    else
    {
        while ((read(file_desc, buffer, sizeof(buffer))) > 0)
        {
            if (buffer[0] == '\n')
            {
                current_line++;
                if (current_line == tracker_no)
                {
                    found = true;
                    break;
                }
            }
        }
    }

    if (!found)
    {
        close(file_desc);
        fatal("Tracker info not found for the given tracker number");
    }
    */

    // Read by line number from
    char buffer[1];
    string tracker_info = "";
    while ((read(file_desc, buffer, sizeof(buffer))) > 0)
    {
        tracker_info += buffer[0];
        if (buffer[0] == '\n')
            break;
    }
    close(file_desc);

    // seperate tracker ip:port
    stringstream ipport(tracker_info);
    string intermediate;
    vector<string> tokens;

    while (getline(ipport, intermediate, ':'))
        tokens.push_back(intermediate);

    tracker_ip = tokens[0];
    tracker_port = stoi(tokens[1]);
}

vector<string> split_string(string address, string delim = ":")
{
    vector<string> res;

    int pos = 0;
    while ((pos = address.find(delim)) != string::npos)
    {
        string t = address.substr(0, pos);
        res.push_back(t);
        address.erase(0, pos + delim.length());
    }
    res.push_back(address);

    return res;
}

void *handle_input(int clientSocket, int trackerSocket)
{
    string input;

    while (true)
    {
        cout << "~>";
        getline(cin, input);
        if (input == "quit")
        {
            if (clientSocket >= 0)
                close(clientSocket);
            close(trackerSocket);
            cout << "Tracker closed" << endl;
            exit(0);
        }
    }
}

// functions to handle user input received from client
// create user
int create_user(vector<string> info)
{
    string user_id = info[1];
    string password = info[2];

    if (userInfo.find(user_id) == userInfo.end())
        userInfo.insert({user_id, password});
    else
        return -1;

    return 0;
}

// login
int login(vector<string> info)
{
    string user_id = info[1];
    string password = info[2];

    if (userInfo.find(user_id) == userInfo.end() || userInfo[user_id] != password)
        return -1;

    if (isLoggedIn.find(user_id) == isLoggedIn.end())
        isLoggedIn.insert({user_id, true});
    else
    {
        if (isLoggedIn[user_id])
            return 1;
        else
            isLoggedIn[user_id] = true;
    }

    return 0;
}

// create group + set creator as admin to accept requests
int create_group(vector<string> info, int client_socket, string client_uid)
{
    if (info.size() != 2)
    {
        send_message(client_socket, "ARG_ERR");
        return -1;
    }
    for (auto group : groups)
    {
        if (group == info[1])
            return -1;
    }

    groups.push_back(info[1]);
    groupAdmins.insert({info[1], client_uid});
    groupMembers[info[1]].insert(client_uid);

    return 0;
}

// list all groups in the network
void list_groups(vector<string> info, int client_socket){
    if(info.size() != 1){
        send_message(client_socket, "ARG_ERR");
        return;
    }

    if(groups.size() == 0){
        send_message(client_socket, "No groups found");
        return;
    }

    string response = "";
    for(auto group: groups){
        response += group + "$$";
    }
    response = response.substr(0, response.length()-2);
    send_message(client_socket, response);
    return;
}

// join group
void join_group(vector<string> info, int socket, string client_uid){
    if(info.size() != 2){
        send_message(socket, "ARG_ERR");
        return;
    }

    if(groupAdmins.find(info[1]) == groupAdmins.end()){
        send_message(socket, "Invalid group ID");
    }
    else if(groupMembers[info[1]].find(client_uid) == groupMembers[info[1]].end()){
        pendingRequests[info[1]].insert(client_uid);
        send_message(socket, "Join request sent");
    }
    else{
        send_message(socket, "You are already a member of this group");
    }
    return;
}

// leave group
void leave_group(vector<string> info, int socket, string client_uid){
    if(info.size() != 2){
        send_message(socket, "ARG_ERR");
        return;
    }

    if(groupAdmins.find(info[1]) == groupAdmins.end())
        send_message(socket, "Invalid group ID");
    
    else if(groupMembers[info[1]].find(client_uid) != groupMembers[info[1]].end()){
        if(groupAdmins[info[1]] == client_uid)
            send_message(socket, "Admin cannot leave the group");
        
        else{
            groupMembers[info[1]].erase(client_uid);
            send_message(socket, "Group left success");
        }
    }
    else
        send_message(socket, "You are not a member of this group");
    
    return;
}

// list pending group join requests
void list_requests(vector<string> info, int socket, string client_uid){
    if(info.size() != 2){
        send_message(socket, "ARG_ERR");
        return;
    }

    if(groupAdmins.find(info[1])==groupAdmins.end() || groupAdmins[info[1]] != client_uid){
        send_message(socket, "Only admins can view the requests");
    }
    else if(pendingRequests[info[1]].size() == 0){
        send_message(socket, "No pending requests");
    }
    else {
        string response = "";
        for(auto i = pendingRequests[info[1]].begin(); i!= pendingRequests[info[1]].end(); i++){
            response += string(*i) + "$$";
        }
        response = response.substr(0, response.length()-2);
        send_message(socket, response);
    }
    return;
}

// accept group joining request
void accept_request(vector<string> info, int socket, string client_uid){
    if(info.size() != 3){
        send_message(socket, "ARG_ERR");
        return;
    }

    if(groupAdmins.find(info[1]) == groupAdmins.end()){
        send_message(socket, "Invalid group ID");
    }
    else if(groupAdmins.find(info[1])->second == client_uid){
        pendingRequests[info[1]].erase(info[2]);
        groupMembers[info[1]].insert(info[2]);
        send_message(socket, "Request accepted");
    }
    else{
        send_message(socket, "Only admin of the group can accept requests");
    }
    return;
}

// list all files in the group
void list_files(vector<string> info, int socket){
    if(info.size() != 2){
        send_message(socket, "ARG_ERR");
        return;
    }

    if(groupAdmins.find(info[1]) == groupAdmins.end()){
        send_message(socket, "Invalid group ID");
    }
    else if(seederList[info[1]].size() == 0){
        send_message(socket, "No files found");
    }
    else{

        string response = "";

        for(auto i: seederList[info[1]]){
            response += i.first + "$$";
        }
        response = response.substr(0, response.length()-2);

        send_message(socket, response);
    }
}

// stop file sharing
void stop_share(vector<string> info, int socket, string client_uid){
    if(info.size() != 3){
        send_message(socket, "ARG_ERR");
        return;
    }
    if(groupAdmins.find(info[1]) == groupAdmins.end())
        send_message(socket, "Invalid group ID");
    
    else if(seederList[info[1]].find(info[2]) == seederList[info[1]].end())
        send_message(socket, "File not yet shared in the group");
    
    else{
        seederList[info[1]][info[2]].erase(client_uid);
        if(seederList[info[1]][info[2]].size() == 0){
            seederList[info[1]].erase(info[2]);
        }
        send_message(socket, "File sharing stopped");
    }
}

// show downloads
void show_downloads(vector<string> info, int socket){
    if(info.size() != 1)
        send_message(socket, "ARG_ERR");
    return;
}

// upload file
void upload_file(vector<string> info, int socket, string client_uid){
    // string filepath = "../client/" + info[1];
    if(info.size() != 3)
        send_message(socket, "ARG_ERR");
    
    else if(groupMembers.find(info[2]) == groupMembers.end())
        send_message(socket, "Invalid Group ID");
    
    else if(groupMembers[info[2]].find(client_uid) == groupMembers[info[2]].end())
        send_message(socket, "You are not a member of the group");
    
    else if(access(info[1].c_str(), F_OK) != 0)
        send_message(socket, "File not found");
    
    else{
        char fileDetails[524288] =  {0};
        send_message(socket, "Uploading...");

        if(read(socket , fileDetails, 524288)){
            if(string(fileDetails) == "error") return;

            vector<string> fdet = split_string(string(fileDetails), "$$");
            //fdet = [filepath, peer address, file size] 
            string filename = split_string(string(fdet[0]), "/").back();
            
            if(seederList[info[2]].find(filename) != seederList[info[2]].end()){
                seederList[info[2]][filename].insert(client_uid);
            }
            else{
                seederList[info[2]].insert({filename, {client_uid}});
            }
            fileSize[filename] = fdet[2];
            
            send_message(socket, "File Uploaded");
        }
    }
    return;
}

void download_file(vector<string> info, int client_socket, string client_uid){
    if(info.size() != 4){
        send_message(client_socket, "ARG_ERR");
    }
    else if(groupMembers.find(info[1]) == groupMembers.end()){
        send_message(client_socket, "Group doesn't exist");
    }
    else if(groupMembers[info[1]].find(client_uid) == groupMembers[info[1]].end()){
        send_message(client_socket, "You are not a member of this group");
    }
    else{
        if(!access(info[3].c_str(), F_OK)){
            send_message(client_socket, "Directory not found");
            return;
        }

        char fileDetails[524288] =  {0};
        // fileDetails = [filename, destination, group id]
        send_message(client_socket, "Downloading...");

        if(read(client_socket , fileDetails, 524288)){
            vector<string> fdet = split_string(string(fileDetails), "$$");
            
            string response = "";
            if(seederList[info[1]].find(fdet[0]) != seederList[info[1]].end()){
                for(auto i: seederList[info[1]][fdet[0]]){
                    if(isLoggedIn[i]){
                        response += userport[i] + "$$";
                    }
                }
                response += fileSize[fdet[0]];
                send_message(client_socket, response);
                            
                seederList[info[1]][info[2]].insert(client_uid);
            }
            else{
                send_message(client_socket, "File not found");
            }
        }
    }
    return;
}

void handle_client(int socket)
{
    string client_uid = "";
    string client_gid = "";

    // User *current = NULL;
    // int newSocket = *((int *)param);
    // cout << "Listening to " << newSocket << endl;

    // Communication loop
    while (true)
    {
        char inputline[SIZE] = {0};

        if (read(socket, inputline, SIZE) <= 0)
        {
            isLoggedIn[client_uid] = false;
            close(socket);
            break;
        }

        istringstream iss(inputline);
        vector<string> commands;
        string token;

        while (getline(iss, token, ' '))
            commands.push_back(token);

        if (commands[0] == "create_user")
        {
            if (commands.size() != 3)
                send_message(socket, "Invalid number of arguments");

            else
            {
                if (create_user(commands) < 0)
                    send_message(socket, "User already exists");
                else
                    send_message(socket, "User created");
            }
        }

        else if (commands[0] == "login")
        {
            if (commands.size() != 3)
                send_message(socket, "ARG_ERR");

            else
            {
                int flag = 0;
                if ((flag = login(commands)) < 0)
                    send_message(socket, "Invalid userID / password");

                else if (flag > 0)
                    send_message(socket, "You already have one active session");

                else
                {
                    send_message(socket, "Login Success");
                    client_uid = commands[1];
                    char buff[96];
                    read(socket, buff, 96);
                    string peerAddress = string(buff);
                    userport[client_uid] = peerAddress;
                }
            }
        }

        else if (commands[0] == "logout")
        {
            isLoggedIn[client_uid] = false;
            send_message(socket, "Logout Success");
        }

        else if (commands[0] == "create_group")
        {
            if (create_group(commands, socket, client_uid) >= 0)
            {
                client_gid = commands[1];
                send_message(socket, "Group created");
            }
            else
                send_message(socket, "Group already exists");
        }

        else if (commands[0] == "list_groups")
            list_groups(commands, socket);

        else if (commands[0] == "join_group")
            join_group(commands, socket, client_uid);

        else if (commands[0] == "leave_group")
            leave_group(commands, socket, client_uid);

        else if (commands[0] == "list_requests")
            list_requests(commands, socket, client_uid);

        else if (commands[0] == "accept_request")
            accept_request(commands, socket, client_uid);

        else if (commands[0] == "list_files")
            list_files(commands, socket);

        else if (commands[0] == "stop_share")
            stop_share(commands, socket, client_uid);

        // else if (commands[0] == "show_downloads")
        //     show_downloads(commands, socket);

        else if (commands[0] == "upload_file")
            upload_file(commands, socket, client_uid);

        else if (commands[0] == "download_file")
            download_file(commands, socket, client_uid);

        else
            send_message(socket, "Invalid command");
    }

    close(socket);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
        fatal("Invalid number of arguments");

    // read tracker details from tracker_info.txt
    const char *filename = argv[1];
    int tracker_no = stoi(argv[2]);

    if (tracker_no != 1)
        fatal("Only 1 tracker available for now");

    string tracker_ip = "";
    int tracker_port = 0;
    get_tracker_info(filename, tracker_no, tracker_ip, tracker_port);
    // cout << tracker_ip << " : " << tracker_port << endl;

    // establish connection
    int trackerSocket = -1, clientSocket = -1;
    struct sockaddr_in serverAddr;
    int opt = 1;
    // struct sockaddr_in clientAddr;
    socklen_t addrSize = sizeof(serverAddr);
    // char buffer[SIZE];

    // Create the server socket
    if ((trackerSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        fatal("Error in socket connection");

    if (setsockopt(trackerSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        fatal("setsockopt");

    if (inet_pton(AF_INET, &tracker_ip[0], &serverAddr.sin_addr) <= 0)
        fatal("Invalid IP address");

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(tracker_port);
    // serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind the server socket to the specified address and port
    if (bind(trackerSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        fatal("Error in binding");

    // Listen for incoming connections
    if (listen(trackerSocket, 20) == 0)
        cout << "Listening for clients..." << endl;
    else
        fatal("Error listening");

    // thread to handle commands on tracker-side
    thread inputThread(handle_input, clientSocket, trackerSocket);

    vector<thread> threads;
    while (true)
    {
        struct sockaddr_in serverAd;
        struct sockaddr_storage serverSt;
        socklen_t addr_size = sizeof(serverSt);

        // get first connection from queue
        if ((clientSocket = accept(trackerSocket,
                                   (struct sockaddr *)&serverSt,
                                   &addr_size)) < 0)
            error("Error accepting client connection");

        threads.push_back(thread(handle_client, clientSocket));
    }

    for (auto it = threads.begin(); it != threads.end(); it++)
        if (it->joinable())
            it->join();

    return 0;
}