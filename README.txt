First use the makefile to compile the client.cpp as well as server.cpp then use the following steps to 
run the server and different clients. For running server we need IP and PORT number and for running 
client we need Username, Server IP and Server PORT.

For Server:
./server <Loopback Address> <PORT NO>

Use a different terminal and go to the same folder in it and start the client as follows
./client <User Name> <Server Address> <PORT NO>

example:

./server 127.0.0.1 8910

./client ana 127.0.0.1 8910


client.cpp Implementation Details:

We use getaddrinfo() to get the structure addrinfo.
We use socket() for getting file descriptors.
And using the connect() API we connect to the server.

Once we make the TCP connection, we start preparing the SBCP packet for the JOIN request, we then
use the pack function provided in the Beej Guide to pack them in network byte order and use the
send() API to send the request to the server.

We add the socket that we use for communication with the server as well as STDIN in the read_fds set.
We use the Select API to check if there is any content to be read in the file descriptors. 
First we receive either an ACK or NAK from the server, depending on which we decide whether to 
continue running or not. We use the unpack() function to convert from the network byte order to
normal byte order and store. If we receive a NAK, we also receive teh reason for the rejection of the request,
we print it to the console and exit the client.

If we receive an ACK, we also receive the number of clients that are currently active as well as
their username, we display it to the console.

When we receive a message from the console we create a chat packet of type SEND and pack it 
into a buffer and send it to the server, the server again uses FWD type packets to send the chat
to other clients. We also receive other types of messages like certain user is ONLINE, certain user 
is IDLE, certain user is OFFLINE, we display them to the console.

We use the ctrl+c command to exit, the client sends a packet of 0 content, the server checks if
the number of bytes received are 0 from a particular client, it closes the connection for that
file descriptor and removes the username from the list and updates it to other clients.


server.cpp Implementation Details:

We use getaddrinfo() to get the structure addrinfo.
We use socket() for getting file descriptors.
We use bind() to associate that socket with a port.
We use listen() for socket connections and limit the queue of incoming connections.
Every time we have a new File Descriptor, add the file descriptor into select() read_fds set.
select() API checks if there is something to read,

It could either be a new client seeking a connection with a JOIN request
 In this case the server checks if client limit is exceeded, if it is then
 the server responds with a NAK, again using the same SBCP packet and Pack() function as described 
 in the client implementation part. The server sends a reason for NAK.
 
 If the username has already been taken by another client, the server sends a NAK and also mentions
 the reason.
 
 Otherwise the server accepts the connection and sends an ACK, it also sends the count of existing clients(inclusive)
 and also a list of user names(exclusive). The server also sends an ONLINE message to all other clients and lets them
 know about the arrival of the new client with it's user name.

 or It could be an existing client with a chat/IDLE, in each cases the server creates a SBCP
 packet of types FWD or IDLE respectively and sends it to all other clients.
 
 When a particular client exits, the server also sends OFFLINE message to other clients, it has been described
 in the client implementation details.
 
Both the client and server discard any message which do not align with the SBCP protocol specifications.
 
 Errata: 

1. We implemented the code on Mac which is also UNIX based system but if we run the same code on PUTTY it behaves differently.

2. Sometimes the messages that we see on terminal has older messages in the end which shows that buffer message are overwritten. 
We spent so much time on this and fixed this issue  but there is still some cases where it appears.



