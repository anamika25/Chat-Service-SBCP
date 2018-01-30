First use the makefile to compile the client.cpp as well as server.cpp then use the following steps to run the server and different clients. For running server we need IP and PORT number and for running client we need Username, Server IP and Server PORT.

For Server:
./server <Loopback Address> <PORT NO>

Use a different terminal to start the client as follows
./client <User Name> <Server Address> <PORT NO>

example:

./server 127.0.0.1 8910

./client ana 127.0.0.1 8910


Client Implementation Details:

getaddrinfo(): To get the structure addrinfo.
socket(): To get the file descriptors.
connect(): To connect to the server.

Once the TCP connection is established, we can start preparing the SBCP packet for the JOIN request, then use the pack function provided in the Beej Guide to pack them in network byte order and use the send() API to send the request to the server.

For communication with the server, we add the socket as well as STDIN in the read_fds set. Then the Select API checks if there is any content to be read in the file descriptors. 
First we receive either an ACK or NAK from the server, depending on that, it is decided whether to continue running or not. unpack() function converts from network byte order to normal byte order and stores. If we receive a NAK, we also receive the reason for the rejection of the request, which gets printed to the console and removes the client.

If we receive an ACK, we also receive the number of clients that are currently active as well as their username, which will be displayed on the console.

When we receive a message from the console we create a chat packet of type SEND and pack it into a buffer and send it to the server, the server again uses FWD type packets to send the chat to other clients. We also receive other types of messages like certain user is ONLINE, certain user is IDLE, certain user is OFFLINE, we display those messages on the console.

We use the ctrl+c command to exit, the client sends a packet of 0 content, the server then checks if the number of bytes received are 0 from a particular client, so it closes the connection for that file descriptor and removes the username from the list and sends an update to other clients.


Server Implementation Details:

getaddrinfo(): To get the structure addrinfo.
socket(): To getting file descriptors.
bind(): To associate that socket with a port.
listen(): For socket connections and to limit the queue of incoming connections.

Every time we have a new File Descriptor, add the file descriptor into select() read_fds set.select() API checks if there is something to read,

It could either be a new client seeking a connection with a JOIN request. In that case, the server checks if client limit is exceeded, if so, then the server responds with a NAK, again using the same SBCP packet and Pack() function as described in the client implementation part. The server then sends a reason for NAK.
 
If the username has already been taken by another client, the server sends a NAK and also mentions the reason.

Otherwise the server accepts the connection and sends an ACK, it also sends the count of existing clients(inclusive) and also a list of user names(exclusive). The server sends an ONLINE message to all other clients to let them know about the arrival of the new client with the user name. Or It could be an existing client with a chat/IDLE, for each, the server creates a SBCP packet of types FWD or IDLE respectively and sends it to all other clients.
When a particular client exits, the server sends OFFLINE message to other clients, which has been described in the client implementation details.
 
Both the client and server discard any message which do not align with the SBCP protocol specifications.