/*
 * This is sample code generated by rpcgen.
 * These are only templates and you can use them
 * as a guideline for developing your own functions.
 */

#include "sxfs.h"
#include "peer_info.h"
#include <sstream>
using namespace std;


static map<pair<string,int>,string> clientList;
static map<pair<string,int>,string >::iterator it = clientList.begin();


void outputClientList() {
	cout << "outputing server list:" << endl;
	int pos = -1;
	std::string delimiter = ".txt";
	std::string name = "";
	client_file_list f_list;
	map<pair<string,int>, string >::iterator iter;
	for (iter = clientList.begin(); iter != clientList.end(); ++iter) {
		cout << iter->first.first << ":" << iter->first.second << " ";
		//converting string to type of client_file_list for populating
		f_list = new char[(iter->second).length() +1];
		strcpy(f_list,iter->second.c_str());
		//extracting filenames from the file_list
		string remaining_list(f_list, strlen(f_list));
		pos = -1;
		name = "";
		while(strcmp(remaining_list.c_str(),"") !=0) {
			pos = remaining_list.find(delimiter);
			name = remaining_list.substr(0, pos+4);   //returning filename
			remaining_list = remaining_list.substr(pos + 4);  //returning remaining filenames
			cout << name << " " ;
		}
		cout << endl;
	}
}

node_list *
file_find_1_svc(char *arg1,  struct svc_req *rqstp)
{
	static node_list result;
	result.node_list_val = new node[clientList.size()];
	result.node_list_len = clientList.size();
	client_file_list f_list;
	int pos = -1;
	int len = 0;
	cout << "node list is: \n";
	map<pair<string,int>, string >::iterator iter;
 	for (iter = clientList.begin(); iter != clientList.end(); ++iter) {
		f_list = new char[(iter->second).length() +1];
		strcpy(f_list,iter->second.c_str());
		string remaining_list(f_list, strlen(f_list));
		pos = remaining_list.find(arg1);
		if(pos >= 0){
			node *temp_node = result.node_list_val + len;
			temp_node->ip= new char[MAXIP];
			strcpy(temp_node->ip,iter->first.first.c_str());
			temp_node->port = iter->first.second;
			cout << temp_node->ip << ":" << temp_node->port << "\n";
			len++;
			continue;
		}
	}
	return &result;
}

int *
update_list_1_svc(IP arg1, int arg2, client_file_list arg3, struct svc_req *rqstp)
{
	static int  result;

	stringstream self_file_list;
	self_file_list << arg3;
	map<pair<string,int>,string >::iterator find_itr;
	find_itr = clientList.find(make_pair(arg1,arg2));
	//add client to client list only if previously not existing
	if(find_itr == clientList.end()) {
		clientList.insert (it, std::pair<pair<string,int>,string >(make_pair(arg1,arg2), self_file_list.str()));
		it++;
	}

	/*TODO: removal of client from list when it fails
	map<pair<char *,int>,string >::iterator find_itr;
	find_itr = clientList.find(make_pair(ip,arg2));
	if(find_itr != clientList.end()) {
		clientList.erase(it);
	}
	 */
	outputClientList();
	return &result;
}
