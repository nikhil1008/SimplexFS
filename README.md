# SimplexFS
Project for Distributed system course

To compile (RPCGEN has already been used to generate files, so that compilation can be ignored):
```
make clean
make
```

How to use:
````
Server: Launch the RPC server with: ./serverside server_port
Client: Launch the client application with: ./clientside “local ip” “server ip” "client_tcp_port" "client_crash_check_port" "server_crash_check_port"
````
Assumptions: 
````
The filenames do not contain any white spaces
All filenames are unique
Hidden files are not considered in the filesystem
````
Options on client side (find_file | download | get_load | update_list | remove_client) where each has its function as follows -

1) find_file will returns the list of nodes which store a file
2) download will download a given file; once downloaded it becomes shareable from that peer
3) get_load returns the load at a given peer requested from another peer
4) update_list provides the list of files stored at a given peer to the server
5) remove_client informs server to remove client's entry from list and disconnects the client from server and peers
