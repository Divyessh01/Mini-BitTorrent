#include <iostream>
#include <vector>
#include <iomanip>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <thread>
#include <sys/stat.h>
#include <unordered_map>

using namespace std;
#define SIZE 524288
#define BUFFSIZE 32768

string client_ip, tracker_ip;
int client_port, tracker_port;
bool is_logged_in = false;

unordered_map<string, string> downloadedFiles;
unordered_map<string, unordered_map<string, bool>> isUploaded;
unordered_map<string, string> fileToFilePath;


void error(string err)
{
    cout << err << endl;
}

void shout(string say)
{
    error(say);
}

void fatal(string err)
{
    perror(err.c_str());
    exit(1);
}

void print_vector(vector<string> words)
{
    cout << "================" << endl;
    for (auto word : words)
        cout << word << endl;
    cout << "================" << endl;
}

void split_ipport(string given, string &ip, int &port)
{
    stringstream check(given);
    vector<string> tokens;

    string intermediate;

    // Tokenizing w.r.t. space ':'
    while (getline(check, intermediate, ':'))
        tokens.push_back(intermediate);

    ip = tokens[0];
    port = stoi(tokens[1]);
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

void get_tracker_info(string filename, string &tracker_ip, int &tracker_port)
{
    int file_desc = open(filename.c_str(), O_RDONLY);
    if (file_desc == -1)
        fatal("Error opening file");

    char buffer[1];
    int current_line = 1;
    bool found = false;
    string tracker_info;

    // Read and print the desired line
    while ((read(file_desc, buffer, sizeof(buffer))) > 0)
    {
        tracker_info += buffer[0];
        if (buffer[0] == '\n')
            break;
    }
    close(file_desc);

    split_ipport(tracker_info, tracker_ip, tracker_port);
}

string get_user_input()
{
    string input;
    getline(cin, input);
    return input;
}

// split lines into words | store words to vector words
// void tokenize(vector<string> &words, string line)
// {
//     char *temp = (char *)malloc((line.size() + 1) * sizeof(char));
//     strcpy(temp, line.c_str());

//     char *word = strtok(temp, " \t");
//     while (word != nullptr)
//     {
//         words.push_back(word);
//         word = strtok(nullptr, " \t");
//     }

//     free(temp);
// }

// connect to tracker
void connect_to_tracker(struct sockaddr_in &serv_addr, int socket)
{

    if (inet_pton(AF_INET, tracker_ip.c_str(), &(serv_addr.sin_addr)) <= 0)
        fatal("Invalid tracker IP address");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(tracker_port);

    // Connect to the tracker
    if (connect(socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        fatal("Error in connection");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(tracker_port);
}

// handle client request | call from new threads created when acting as server
void handle_client_request(int client_socket)
{
    // yet to be implemented
    return;
}

// runs as a server listening to other clients
void *act_as_server(void *arg)
{
    int server_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int opt = 1;

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        fatal("seversocket");

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        fatal("setsockopt");

    address.sin_family = AF_INET;
    address.sin_port = htons(client_port);

    if (inet_pton(AF_INET, client_ip.c_str(), &address.sin_addr) <= 0)
        fatal("Invalid client_ip");

    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
        fatal("bind failure");

    if (listen(server_socket, 3) < 0)
        fatal("listen failure");

    shout("Listening...");

    vector<thread> threads;
    while (true)
    {
        int client_socket;

        if ((client_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            fatal("Acceptance error");

        shout("Connection Accepted");

        threads.push_back(thread(handle_client_request, client_socket));
    }
    for (auto it = threads.begin(); it != threads.end(); it++)
        if (it->joinable())
            it->join();

    close(server_socket);
}

// commands to be processed client side
// list groups
void list_groups(int socket, string response)
{
    if (response == "No groups found")
    {
        shout(response);
        return;
    }

    vector<string> groups = split_string(response, "$$");
    shout("====================");
    shout("list of groups: ");
    for (auto group : groups)
        shout(group);

    shout("====================");
    return;
}

// list group join requests
void list_requests(int socket, string response)
{
    if (string(response) == "Only admins can view the requests" or string(response) == "No pending requests")
    {
        shout(response);
        return;
    }

    vector<string> requests = split_string(response, "$$");
    shout("====================");
    shout("Group join requests:");
    for (auto request : requests)
        shout(request);

    shout("====================");

    return;
}

// common function to print server response for: leave group | create user | join group | accept request
void default_command(string response)
{
    shout(response);
    return;
}

// list all files available in group
void list_files(int socket, string response)
{
    if (response == "Invalid group ID" or response == "No files found")
    {
        shout(response);
        return;
    }

    vector<string> files = split_string(response, "$$");

    shout("====================");
    shout("List of files:");
    for (auto file : files)
        shout(file);

    shout("====================");
    return;
}

void stop_share(vector<string> info, string response)
{
    shout(response);
    isUploaded[info[1]].erase(info[2]);
    return;
}

// get file size
long long file_size(string filepath)
{
    struct stat fileStat;

    if (stat(filepath.c_str(), &fileStat) == 0)
        return fileStat.st_size;

    shout("File not found");
    return -1;
}

// upload file
void upload_file(vector<string> info, int sock, string response)
{
    if(response == "Invalid Group ID" or response == "You are not a member of the group" or response == "File not found"){
        shout(response);
        return;
    }

    string fileDetails = "";
    string filepath = info[1];

    string filename = split_string(string(filepath), "/").back();

    if (isUploaded[info[2]].find(filename) != isUploaded[info[2]].end())
    {
        shout("File already uploaded");
        if (send(sock, "error", 5, MSG_NOSIGNAL) == -1)
        {
            perror("Error:");
            return;
        }
        return;
    }
    else
    {
        isUploaded[info[2]][filename] = true;
        fileToFilePath[filename] = string(filepath);
    }

    string filesize = to_string(file_size(filepath));

    fileDetails += string(filepath) + "$$";
    fileDetails += string(client_ip) + ":" + to_string(client_port) + "$$";
    fileDetails += filesize + "$$";

    if (send(sock, fileDetails.c_str(), fileDetails.length(), MSG_NOSIGNAL) == -1)
    {
        perror("Error:");
        return;
    }

    char server_reply[BUFFSIZE] = {0};
    read(sock, server_reply, BUFFSIZE);
    shout(server_reply);

    return;
}

void download_file(vector<string> info, int socket, string response)
{
    if(response == "Group doesn't exist" or "You are not a member of this group" or "Directory not found"){
        shout(response);
        return;
    }

    string fileDetails = "";
    fileDetails += info[2] + "$$";
    fileDetails += info[3] + "$$";
    fileDetails += info[1];
    // fileDetails = [filename, destination, group id]

    if (send(socket, fileDetails.c_str(), fileDetails.length(), MSG_NOSIGNAL) == -1)
    {
        perror("Error:");
        return;
    }

    char server_response[SIZE] = {0};
    read(socket, server_response, SIZE);

    if (string(server_response) == "File not found")
    {
        shout(server_response);
        return;
    }
    vector<string> peersWithFile = split_string(server_response, "$$");

    // empty the buffer
    for (int i = 0; i < sizeof(server_response); ++i)
        server_response[i] = 0;

    read(socket, server_response, SIZE);

    return;
}

void execute_commands(vector<string> commands, int socket)
{
    char server_response[BUFFSIZE] = {0};
    read(socket, server_response, BUFFSIZE);
    // cout << "server response: " << server_response << endl;

    if (string(server_response) == "ARG_ERR")
    {
        shout("Invalid number of arguments for the command");
        return;
    }

    if (commands[0] == "login" and !is_logged_in)
    {
        shout(server_response);
        if (string(server_response) == "Login Success")
        {
            is_logged_in = true;
            string peerAddress = client_ip + ":" + to_string(client_port);
            write(socket, &peerAddress[0], peerAddress.length());
        }
    }
    else if (commands[0] == "logout")
    {
        default_command(server_response);
        is_logged_in = false;
    }

    else if (commands[0] == "create_user")
    {
        default_command(server_response);
    }

    else if (commands[0] == "create_group")
    {
        default_command(server_response);
    }

    else if (commands[0] == "join_group")
    {
        default_command(server_response);
    }

    else if (commands[0] == "list_groups")
        list_groups(socket, server_response);

    else if (commands[0] == "list_requests")
        list_requests(socket, server_response);

    else if (commands[0] == "accept_request")
        default_command(server_response);

    else if (commands[0] == "leave_group")
        default_command(server_response);

    else if (commands[0] == "list_files")
        list_files(socket, server_response);

    else if (commands[0] == "stop_share")
        stop_share(commands, server_response);

    // else if (commands[0] == "show_downloads")
    //     show_downloads();

    else if (commands[0] == "upload_file")
        upload_file(commands, socket, server_response);

    else if (commands[0] == "download_file")
        download_file(commands, socket, server_response);

    else
        default_command("Invalid command");

    return;
}

int main(int argc, char *argv[])
{

    if (argc != 3)
        fatal("Invalid number of arguments");

    string ip_port = argv[1];  // client info
    string filename = argv[2]; // tracker_info.txt

    split_ipport(ip_port, client_ip, client_port);        // split ip and port
    get_tracker_info(filename, tracker_ip, tracker_port); // get trackers ip and port from tracker_info.txt | split ip_port invoked internally

    // client and tracker ip + port
    // cout << "client : " << client_ip << " : " << client_port << endl;
    // cout << "Tracker : " << tracker_ip << " : " << tracker_port << endl;

    int clientSocket = -1;
    struct sockaddr_in serv_addr;

    // Create the client socket
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        fatal("Error in socket creation");

    pthread_t *newthread = new pthread_t();
    if (pthread_create(newthread, NULL, act_as_server, NULL) != 0)
        fatal("Thread creation failure");
    if (pthread_detach(*newthread) != 0)
        error("Error in thread detachment");

    delete newthread;

    connect_to_tracker(serv_addr, clientSocket);

    vector<string> commands;
    while (true)
    {
        commands.clear();
        cout << "~>";
        string input = get_user_input();

        commands = split_string(input, " ");
        // print_vector(commands);

        if (commands[0] == "login" and is_logged_in)
        {
            error("You already have one active session");
            continue;
        }
        if (commands[0] != "login" and commands[0] != "create_user" and !is_logged_in)
        {
            error("Require user login");
            continue;
        }

        if (send(clientSocket, input.c_str(), input.length(), MSG_NOSIGNAL) == -1)
            fatal("Error:");

        execute_commands(commands, clientSocket);
    }

    close(clientSocket);

    return 0;
}