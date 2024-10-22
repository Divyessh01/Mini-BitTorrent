# Assignment - 04

## Overview :

* Peer to peer file sharing system:
    * A group based file sharing system where users can share, download files from the group they belong to.

***


## Execution :

### compilation :

* move to tracker directory & execute the following command:
    ```
    g++ tracker.cpp -o tracker -lpthread
    ```

* move to client directory & execute the following command:
    ```
    g++ client.cpp -o client -lpthread
    ```

### Start:
* First start the tracker with following command:
    ```
    ./tracker <tracker_info_file> <tracker_no>
    eg. ./tracker tracker_info.txt 1
    ```

    * First argument contains filepath, which contains the trackers information as: 
        * <tracker_ip>:<tracker_port>

    * Second argument specifies the tracker number.
    
* Start the client with the command:
    ```
    ./client <client_ip:client_port> <tracker_info_file>
    eg. ./client 127.0.0.1:8000 tracker_info.txt
    ```

    * First argument specifies client information in the form : 
        * <client_ip>:<client_port>

    * Second argument contains filepath, containing tracker information as mentioned above.
    
***


## Commands

### Tracker:

* Close the tracker :
    ```
    quit
    ```

### Client:

* Create user account :
    ```
    create_user <user_id> <password>
    ```

* Login :
    ```
    login <user_id> <password>
    ```

* Create group :
    ```
    create_group <group_id>
    ```

* Join group : 
    ```
    join_group <group_id>
    ```

* Leave group :
    ```
    leave_group <group_id>
    ```

* List pending join requests :
    ```
    list_requests <group_id>
    ```

* Accept group joining request :
    ```
    accept_request <group_id> <user_id>
    ```

* List all groups in the network :
    ```
    list_groups
    ```

* List all sharable files in the group :
    ```
    list_files <group_id>
    ```

* Upload file :
    ```
    upload_file <file_path> <group_id>
    ```

* Download file :
    ```
    download_file <group_id> <file_name> <destination_path>
    ```

* Logout :
    ```
    logout
    ```

* Show downloads :
    ```
    show_downloads
    ```
    * output format (D:Downloading | C:Complete):
        ```
        [D] <group_id> <filename>
        [C] <group_id> <filename>
        ```
* Stop sharing :
    ```
    stop_share <group_id> <file_name>
    ```

***

## Issues : 

* Only a single tracker is implemented
* any user in the network can stop file sharing, which should not be the case
* dowload_file, show_downloads are incomplete
* absolute file path is required for upload_file
 

***