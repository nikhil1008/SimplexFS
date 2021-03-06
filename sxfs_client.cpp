/*
 * This is sample code generated by rpcgen.
 * These are only templates and you can use them
 * as a guideline for developing your own functions.
 */

#include "sxfs.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>
#include <iostream>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>
#include "tcp_client.h"
#include "tcp_server.h"
#include "tcp_communication.h"
#include <mutex>
#include <fstream>
#include <sstream>
#include <errno.h>
#include <openssl/md5.h>
#include <map>
#include <vector>
#include <iterator>

#define FS_ROOT "./5105_node_files"

using namespace std;
using std::thread;

class Client;

class Client {

public:
    //thread related
    std::thread tcp_thread;
    std::thread heartbeat_thread;
    std::thread update_thread;
    std::thread fault_check_thread;
    bool update_flag = false;
    bool heartbeat_flag = false;
    bool tcp_flag = false;
    bool fault_check_flag = false;
    void update_thread_func();
    void heartbeat();
    void tcp_thread_func();
    void fault_check_func();

    //class fields
    CLIENT *clnt;
    char *self_ip;
    char *server_ip;
    int self_port;
    int ping_port;
    int server_port;
    int client_number;
    int num_active_clients;
    int dest_latency;
    client_file_list self_file_list;
    node_list *peers_with_file;
    multimap<int, pair<char *, int>> peer_load; //(load,pair<ip,port>)
    TcpClient *tcp_clnt; //create self as tcp client and serv doe peer operations
//    TcpClient *s_fault_check_clnt;
    TcpServer *tcp_serv;
    TcpServer *ping_serv;
    unsigned char checksum[MD5_DIGEST_LENGTH];

    //class methods
    void file_find(char *filename);
    int get_load(char * peer_ip, int peer_port);
    void download(char *filename);
    void populate_file_list();
    void update_list();
    void remove_client();
    void md5sum(char *filename, int size);
    void download_file_helper();
    string finding_destinaton_peer(char *filename);
    void checksum_comparison(string file_to_download, char *original_checksum);

    static bool compare_first(const std::pair<int, pair<char *, int> > &lhs, const std::pair<int, pair<char *, int> > &rhs);
    static bool compare_second(const std::pair<string, int > &lhs, const std::pair<string, int > &rhs);
    static bool compare_equal(const std::pair<int, pair<char *, int> > &lhs, const std::pair<int, pair<char *, int> > &rhs);
    std::vector <string> str_split(const std::string &str, char delimiter);

    Client(char *ip, char *host, int port, int p_port, int s_port) {
        self_ip = ip;
        server_ip = host;
        self_port = port;
        ping_port = p_port;
        server_port = s_port;
        clnt = clnt_create(server_ip, SIMPLE_XFS, SIMPLE_VERSION, "udp");
        if (clnt == NULL) {
            clnt_pcreateerror(server_ip);
            exit(1);
        }
        update_list();
        std::cout << ".....Completed client creation.....\n";
        tcp_serv = new TcpServer(self_port, MAXCLIENTS);
        ping_serv = new TcpServer(ping_port, 1);

        heartbeat_flag = true;
        heartbeat_thread = thread(&Client::heartbeat, this);
        fault_check_flag = true;
        fault_check_thread = thread(&Client::fault_check_func, this);
        update_flag = true;
        update_thread = thread(&Client::update_thread_func,this);
        tcp_flag = true;
        tcp_thread = thread(&Client::tcp_thread_func, this);


        tcp_thread.detach();
        update_thread.detach();
        fault_check_thread.detach();
        heartbeat_thread.detach();
    }

    ~Client() {
        remove_client();
        if (tcp_thread.joinable()) {
            tcp_thread.join();
        }
        if (update_thread.joinable()) {
            update_thread.join();
        }
        if (fault_check_thread.joinable()) {
            fault_check_thread.join();
        }
        if (heartbeat_thread.joinable()) {
            heartbeat_thread.join();
        }
        if (clnt)
            clnt_destroy(clnt);
        free(tcp_clnt);
        free(tcp_serv);
    }
};

bool Client::compare_first(const std::pair<int, pair<char *, int> > &lhs,
                           const std::pair<int, pair<char *, int> > &rhs) {
    return lhs.first < rhs.first;
}

bool Client::compare_second(const std::pair<string, int >  &lhs,
                           const std::pair<string, int >  &rhs) {
    return lhs.second < rhs.second;
}

bool Client::compare_equal(const std::pair<int, pair<char *, int> > &lhs,
                           const std::pair<int, pair<char *, int> > &rhs) {
    return lhs.first == rhs.first;
}

std::vector <string> Client::str_split(const std::string &str, char delimiter) {
    vector <string> strings;
    string tmp = "";
    for (int i = 0; i < str.length(); i++) {
        if (str[i] == delimiter) {
             strings.push_back(tmp);
             tmp = "";
        } else {
             tmp = tmp + str[i];
        }
    }
    strings.push_back(tmp);
    return strings;
}

void Client::heartbeat() {
    //server tries to connect each client every 5 sec
    ping_serv->servListen();
    socklen_t clilen;
    int newsockfd;
    while(heartbeat_flag) {
        clilen = sizeof(ping_serv->cli_addr);
        newsockfd = accept(ping_serv->sockfd, (struct sockaddr *) &(ping_serv->cli_addr), &clilen);
        if (newsockfd < 0) {
            cout << "Could not receive connection from server\n";
        }
    }
}

void Client::fault_check_func() {
    //ping server every 5 sec
    bool crashed_flag = false;
    while (fault_check_flag) { // //TODO: if ping not recvd within 5 sec, => server down
        TcpClient *s_fault_check_clnt = new TcpClient(server_ip, server_port);
        if (s_fault_check_clnt->clntOpen() < 0) {
            cout << "Could not connnect to server\n"; //destroy rpc clnt
            crashed_flag = true;
        } else {
            if (crashed_flag) {
                //recreate rpc clnt and connect to server and update list
                clnt = clnt_create(server_ip, SIMPLE_XFS, SIMPLE_VERSION, "udp");
                if (clnt == NULL) {
                    clnt_pcreateerror(server_ip);
                    exit(1);
                }
                update_list();
                crashed_flag = false;
            }
        }
        s_fault_check_clnt->clntClose();
        free(s_fault_check_clnt);
        sleep(5);
    }
}



void Client::update_thread_func() {
    while(update_flag) {
        sleep(30);
        update_list();
    }
}

void Client::tcp_thread_func() {
    while(tcp_flag){
        tcp_serv->servListen();
        client_number = tcp_serv->servAcceptAndSendLoad();
//        cout << client_number << endl;
        num_active_clients = tcp_serv->getNumActiveClients();
        if(strcmp((tcp_serv->download_flag),"true")==0){
            strcpy(tcp_serv->download_flag,"false");
            download_file_helper();
        }
    }
}


void Client::md5sum(char *filename, int size) {
    unsigned char data[size + 1];
    MD5_CTX ctx;
    FILE *fp = NULL;
    size_t bytes;

    fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "fopen error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    MD5_Init(&ctx);
    while ((bytes = fread(data, 1, size, fp)) != 0)
        MD5_Update(&ctx, data, bytes);
    MD5_Final(checksum, &ctx);
    fclose(fp);
}

void Client::populate_file_list() {
    char temp_list[MAXFILELIST];
    temp_list[0] = '\0';
    int client_num_files = 0;
    DIR *dir;
    struct dirent *entry;
    if ((dir = opendir(FS_ROOT)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, ".", 1) != 0) { //ignoring this directory , parent directory and hidden files
                client_num_files++;
                strcat(temp_list, entry->d_name);
                strcat(temp_list, " "); //appending space for distinguishing filenames in the list
            }
        }
    }
    closedir(dir);
    cout << "populating file list on client side: " << temp_list << endl;
    self_file_list = (client_file_list) temp_list;
}

void Client::file_find(char *filename) {
    peers_with_file = file_find_1(filename, clnt);
    if (peers_with_file == (node_list *) NULL) {
        clnt_perror(clnt, "call failed");
    } else {
        cout << "Node_list for " << filename << " is:\n";
        for (int i = 0; i < peers_with_file->node_list_len; i++) {
            cout << (peers_with_file->node_list_val + i)->ip << ":" << (peers_with_file->node_list_val + i)->port << endl;
        }
        cout << "\n";
    }
}

int Client::get_load(char * peer_ip, int peer_port) {
    char *temp_load;
    int load = 0;
    if ((strcmp(self_ip, peer_ip) != 0) || (self_port != peer_port)) {
        tcp_clnt = new TcpClient(peer_ip,peer_port);
        tcp_clnt->clntOpen();
        tcp_clnt->clntRead(&temp_load);
        string send_download_flag = "false";
        tcp_clnt->clntWrite(send_download_flag.c_str(),send_download_flag.length());
        tcp_clnt->clntClose();
        load = atoi(temp_load);
//        cout << "read load " << load << "\n";
    } else {
        //create self as tcp client and servr does peer operations
        load = num_active_clients;
//        cout << "read self load " << load << "\n";
    }
    return load;
}

string Client::finding_destinaton_peer(char *filename) {
    file_find(filename);
    std::vector<pair<string, int>> minEqualLoad;
    string temp_nodename = "";
    if (peers_with_file->node_list_len == 0) {
        cout << "File does not exist" << endl;
        return "0";
    } else {
        int temp_load;
        map<int, pair<char *, int >>::iterator load_itr = peer_load.begin();
        for (int i = 0; i < peers_with_file->node_list_len; i++) {
            temp_load = get_load((peers_with_file->node_list_val + i)->ip, (peers_with_file->node_list_val + i)->port);
            peer_load.insert(load_itr, std::pair<int, pair<char *, int >>
                    (temp_load, make_pair((peers_with_file->node_list_val + i)->ip,
                                          (peers_with_file->node_list_val + i)->port)));
            load_itr++;
        }
        load_itr = min_element(peer_load.begin(), peer_load.end(), this->compare_first);
        int min_load = load_itr->first;

        //if similar loads
        multimap<int, pair<char *, int >>::iterator find_itr;
        typedef multimap<int, pair<char *, int>>::iterator MMAPIterator;
        std::pair<MMAPIterator, MMAPIterator> result = peer_load.equal_range(min_load);
        std::cout << "All values for key 'equal load' are," << std::endl;
        for (MMAPIterator it = result.first; it != result.second; it++) {
            std::cout << it->second.first << ":" << it->second.second << std::endl;
            minEqualLoad.push_back(make_pair(it->second.first, it->second.second));
        }

        //adding hostnames to  env variables so that latency relation can be established
        for (int i = 0; i < minEqualLoad.size(); i++) {
            string node_name = "N";
            node_name.append(to_string(i));
            string node_name_value = "";
            node_name_value.append(minEqualLoad[i].first).append(":").append(to_string(minEqualLoad[i].second));
            setenv(node_name.c_str(), node_name_value.c_str(), 1);
        }

        //TODO: implement latency in sending
        char latency_relation[255];
        map<std::string, int> latencies;
        char relainfo[MAXFILENAME];
        map<std::string, int>::iterator lat_itr = latencies.begin();
        int lat;
        string left, right;
        for (int i = 0; i <= minEqualLoad.size(); i++) {
            string peername = "N";
            peername.append(to_string(i));
            snprintf(relainfo, MAXFILENAME, "REL%d", i);
            strcpy(latency_relation, getenv(relainfo));
            vector<string> rel_info = str_split(latency_relation, ',');
            left = rel_info[0];
            right = rel_info[1];
            lat = atoi(rel_info[2].c_str());
            if ((strcmp(left.c_str(), "N0") == 0)) {
                if(strcmp(right.c_str(), peername.c_str()) == 0) {
                    latencies.insert(lat_itr, std::pair<string, int>(right, lat));
                    lat_itr++;
                    continue;
                }
            }
        }
        lat_itr = min_element(latencies.begin(), latencies.end(), this->compare_second);
        string peer_details = lat_itr->first;
        dest_latency = lat_itr->second;
        return peer_details;
    }
}

void Client::download_file_helper() {
    char *file_to_download;
    tcp_serv->servRead(client_number, &file_to_download);
//    cout << "File to download: " << *file_to_download << endl;
    ifstream peer_file(file_to_download, std::ifstream::in);
    int size;
    char *peer_file_contents;
    if (peer_file.is_open()) {
        peer_file.seekg(0, peer_file.end);
        size = peer_file.tellg();
        peer_file.seekg(0, peer_file.beg);
        peer_file_contents = new char[size+1];
        peer_file.read(peer_file_contents, size);
        peer_file.close();
        peer_file_contents[size] = '\0';
//        cout << "write: " << peer_file_contents << endl;
        //emulating latency for file download
        usleep(dest_latency*1000);
        if (tcp_serv->servWrite(client_number, peer_file_contents, size+1) != (size+1)) {
            cout << "Unable to transfer full file" << endl;
        }
        delete[] peer_file_contents;
        //recv acknowledgement that contents transferred
        char *acknowledgement;
        tcp_serv->servRead(client_number, &acknowledgement);
        //send md5 checksum value
        md5sum(file_to_download, size);
        std::string orig_checksum(reinterpret_cast<char*>(checksum));
//        cout << "serv check: " << orig_checksum << endl;
        if (tcp_serv->servWrite(client_number, orig_checksum.c_str(), MD5_DIGEST_LENGTH) != MD5_DIGEST_LENGTH) {
            cout << "Unable to transfer checksum" << endl;
        }
        tcp_serv->servRead(client_number, &(tcp_serv->download_flag));
    } else {
        cout << "Unable to open " << file_to_download << " So can not transfer the file contents" << endl;
    }
}

void Client::checksum_comparison(string file_to_download, char *original_checksum){
    char name[MAXFILENAME];
    strcpy(name, file_to_download.c_str());
    int size;
    ifstream downloaded_file(file_to_download, std::ofstream::in);
    if (downloaded_file.is_open()) {
        downloaded_file.seekg(0, downloaded_file.end);
        size = downloaded_file.tellg();
        downloaded_file.seekg(0, downloaded_file.beg);
        downloaded_file.close();
    }
    md5sum(name, size);
    std::string new_checksum(reinterpret_cast<char *>(checksum));
    if (strncmp(original_checksum, new_checksum.c_str(),MD5_DIGEST_LENGTH) == 0) {
        update_list();
    } else {
        cout << "Checksum mismatch, deleting the downloaded file. Please re-download " << endl;
        if (remove(name) != 0)
            perror("Error deleting file");
    }
}

void Client::download(char *filename) {
    string peer_details = finding_destinaton_peer(filename);
    if(strcmp(peer_details.c_str(),"0") != 0) {
        char *peervalue = getenv(peer_details.c_str());
        vector<string> peernodeinfo = str_split(peervalue, ':');
        char dest_ip[MAXIP];
        strcpy(dest_ip, peernodeinfo[0].c_str());
        int dest_port = stoi(peernodeinfo[1]);
//    cout << "dest: " << dest_ip << ":" <<dest_port << endl;
        char *clnt_file_contents;
        string file_to_download = FS_ROOT;
        file_to_download.append("/");
        file_to_download.append(filename);
        //if ip and port same as self, i.e. act like a server
        if ((strcmp(self_ip, dest_ip) != 0) || (self_port != dest_port)) {
            tcp_clnt = new TcpClient(dest_ip, dest_port);
            char *temp;
            if(tcp_clnt->clntOpen()<0){
                // peer crashed.... try to download from another peer
                tcp_clnt->clntClose();
                download(filename);
            }
            else {
                tcp_clnt->clntRead(&temp);
                string send_download_flag = "true";
                if (tcp_clnt->clntWrite(send_download_flag.c_str(), send_download_flag.length()) <
                    send_download_flag.length()) {
                    //failed to send filename to peer... peer crashed.... try to download from another peer
                    tcp_clnt->clntClose();
                    remove_client_1(dest_ip, dest_port, clnt);
                    download(filename);
                }
                else if (tcp_clnt->clntWrite(file_to_download.c_str(), file_to_download.length()) !=
                    file_to_download.length()) {
                    //failed to send filename to peer... peer crashed.... try to download from another peer
                    tcp_clnt->clntClose();
                    remove_client_1(dest_ip, dest_port, clnt);
                    download(filename);
                }
                else {
                    tcp_clnt->clntRead(&clnt_file_contents);

                    string ack = "acknowledged";
                    if (tcp_clnt->clntWrite(ack.c_str(), ack.length()) != ack.length()) {
                        tcp_clnt->clntClose();
                        remove_client_1(dest_ip, dest_port, clnt);
                        download(filename);
                    }
                    else {
                        char *original_checksum;
                        tcp_clnt->clntRead(&original_checksum);
                        send_download_flag = "false";
                        tcp_clnt->clntWrite(send_download_flag.c_str(), send_download_flag.length());
                        tcp_clnt->clntClose();
                        // create a file and write these contents

                        ofstream client_file(file_to_download.c_str(), std::ofstream::out);
                        if (client_file.is_open()) { //if able to open the file
                            client_file << clnt_file_contents;
//                cout << "content received: " << clnt_file_contents << endl;
                            client_file.close();
                            //update list if checksum returned success
                            checksum_comparison(file_to_download, original_checksum);
                        } else {
                            cout << "Unable to create " << filename << endl;
                        }
                    }
                }
            }
        } else {
            ifstream fptr(file_to_download.c_str(), std::ifstream::in);
//            cout << "name file: " << file_to_download <<endl;
            if (fptr.good() == 0) {
                cout << "File already exists" << endl; //not overwriting as not considering consistency issues
            } else {
                strcpy(tcp_serv->download_flag, "true");
            }
        }
    }

}

void Client::update_list() {
    populate_file_list();
    auto result_4 = update_list_1(self_ip, self_port, self_file_list, ping_port, clnt);
    if (result_4 == NULL) {
        clnt_perror(clnt, "call failed");
    }
}

void Client::remove_client(){
    auto result_5 = remove_client_1(self_ip, self_port, clnt);
    if (result_5 == NULL) {
        clnt_perror(clnt, "call failed");
    }
    tcp_flag = false;
    update_flag = false;
    heartbeat_flag = false;
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        std::cout << "Usage: ./clientside client_ip server_ip client_port ping_port server_port\n";
        exit(1);
    }
    char *client_ip = (char *) argv[1];
    char *serv_ip = (char *) argv[2];
    int self_port = stoi(argv[3]);
    int ping_port = stoi(argv[4]);
    int server_port = stoi(argv[5]);

    Client conn(client_ip, serv_ip, self_port, ping_port, server_port);
    char func[1];
    int func_number;
    char search_filename[MAXFILENAME];
    while (1) {
        std::cout << "Please enter what function you want to perform [1-5]:\n"
                  << "Function description\n1 file_find\n2 download\n3 get_load\n4 update_list\n5 remove_client\n";
        std::cin >> func;
        try {
            func_number = stoi(func);
        }
        catch (std::exception &e) {
            cout << "ERROR:  Please limit operation values from 1-4 " << endl;
            continue;
        }
        switch (func_number) {
            case 1:
                std::cout << "Please enter the filename to be searched:\n";
                std::cin.get();
                std::cin.getline(search_filename, MAXFILENAME);
                conn.file_find(search_filename);
                break;
            case 2:
                std::cout << "Please enter the filename to be downloaded:\n";
                std::cin.get();
                std::cin.getline(search_filename, MAXFILENAME);
                conn.download(search_filename);
                break;
            case 3:
                char dest_ip[MAXIP];
                int dest_port;
                char temp_port[4];
                std::cout << "Please enter hostname:\n";
                std::cin >> dest_ip;
                std::cout << "Please enter hostport:\n";
                std::cin >> temp_port;
                try {
                    dest_port = stoi(temp_port);
                }
                catch (std::exception &e) {
                    cout << "ERROR:  invalid port number " << endl;
                    continue;
                }
                cout << "Load is: " << conn.get_load(dest_ip,dest_port) << endl;
                break;
            case 4:
                conn.update_list();
                break;
            case 5:
                conn.remove_client();
                exit(0);
                break;
            default:
                std::cout << "Wrong format specified. Please retry \n";
                break;
        }
    }
}
