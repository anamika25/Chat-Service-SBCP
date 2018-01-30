/*
 * server.cpp
 *
 *  Created on: Sep 20, 2016
 *      Author: anamika
 */




#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <iostream>
#include <string>
#include <set>
#include <map>
#include <stdarg.h>
using namespace std;

// get sockaddr, IPv4/IPv6
void *get_in_addr(struct sockaddr *socket_addr)
{
	if (socket_addr->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)socket_addr)->sin_addr);
	}
	return &(((struct sockaddr_in6*)socket_addr)->sin6_addr);
}

//SBCP attribute type
typedef struct
{
	int16_t type;
	int16_t length;
	union
	{
		char username[16];
		char message[512];
		char reason[32];
		int16_t client_count;
	}payload;
}SBCP_attr;

//SBCP Packet
struct SBCP_packet
{
 int8_t version;
 int8_t type;
 int16_t length;
 SBCP_attr attr;
};

void packi16(char *buf, unsigned int i)
{
 *buf++ = i>>8; *buf++ = i;
}

unsigned int unpacki16(char *buf)
{
	return ((unsigned int)buf[0]<<8) | buf[1];
}

unsigned int pack(char *buf, char *format, ...)
{
	va_list ap;
	int8_t c; // 8-bit
	int16_t h; // 16-bit
	char *s; // strings
	unsigned int len;
	unsigned int size = 0;
	va_start(ap, format);
	for(; *format != '\0'; format++) {
		switch(*format) {

			case 'c': // 8-bit
			size += 1;
			c = (int8_t)va_arg(ap, int); // promoted
			*buf++ = c;
			//*buf++ = (c>>0)&0xff;
			break;

			case 'h': // 16-bit
			size += 2;
			h = (int16_t)va_arg(ap, int);
			packi16(buf, h);
			buf += 2;
			break;

			case 's': // string
			s = va_arg(ap, char*);
			len = strlen(s);
			size += len + 2;
			packi16(buf, len);
			buf += 2;
			memcpy(buf, s, len);
			buf += len;
			break;
		}
	}
	va_end(ap);
	return size;
}

void unpack(char *buf, char *format, ...)
{
	va_list ap;
	int8_t *c; // 8-bit
	int16_t *h; // 16-bit
	char *s;
	unsigned int len, maxstrlen=0, count;
	va_start(ap, format);
	for(; *format != '\0'; format++) {
		switch(*format) {

			case 'c': // 8-bit unsigned
			c = va_arg(ap, int8_t*);
			*c = *buf++;
			break;

			case 'h': // 16-bit
			h = va_arg(ap, int16_t*);
			*h = unpacki16(buf);
			buf += 2;
			break;

			case 's': // string
			s = va_arg(ap, char*);
			len = unpacki16(buf);
			buf += 2;
			if (maxstrlen > 0 && len > maxstrlen)
				count = maxstrlen - 1;
			else
				count = len;
			memcpy(s, buf, count);
			s[count] = '\0';
			buf += len;
			break;

			default:
			if (isdigit(*format)) { // track max str len
				maxstrlen = maxstrlen * 10 + (*format-'0');
			}
		}
		if (!isdigit(*format))
			maxstrlen = 0;
	}
	va_end(ap);
}

bool find_map(map<int,string> m,string username )
{
	map<int,string>::iterator it;
	for(it=m.begin();it!=m.end();it++)
	{
		if(it->second==username)
			return true;
	}
	return false;
}

int main(int argc, char* argv[])
{
	if(argc!=3)
	{
		cout<<"please enter server, server_ip_address, server_port \n"<<endl;
	}
	fd_set master; // master file descriptor list
	fd_set read_fds; // temp file descriptor list for select()
	struct SBCP_packet packet_recv, packet_send;
	//set<string> user_list;
	char user_name[16];
	//char message[512];
	char reason[32];
	int client_count = 0;
	string msg="";
	map<int,string> fd_user;

	int fdmax; // maximum file descriptor number
	int listener; // listening socket descriptor
	int newfd; // newly accept()ed socket descriptor
	struct sockaddr_storage remoteaddr; // client address
	socklen_t addrlen;
	char buf[700]; // buffer for incoming data
	//char buf_send[700]; //buffer for outgoing data
	int nbytes;
	char remoteIP[INET6_ADDRSTRLEN];
	int yes=1; // for setsockopt() SO_REUSEADDR, below
	int i, j, rv;
	struct addrinfo hints, *ai, *p;

	FD_ZERO(&master); // clear the master and temp sets
	FD_ZERO(&read_fds);

	// get us a socket and bind it
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	//hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &ai)) != 0) {
		fprintf(stderr, "select of server: %s\n", gai_strerror(rv));
		exit(1);
	}

	for(p = ai; p != NULL; p = p->ai_next)
	{
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0)
		{
			continue;
		}

		// lose the pesky "address already in use" error message
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
		{
			close(listener);
			continue;
		}
		break;
	}

	// if we got here, it means we didn't get bound
	if (p == NULL)
	{
		fprintf(stderr, "selectserver: failed to bind\n");
		exit(2);
	}

	freeaddrinfo(ai); // all done with this

	// listen
	if (listen(listener, 100) == -1)
	{
		perror("listen");
		exit(3);
	}

	// add the listener to the master set
	FD_SET(listener, &master);
	// keep track of the biggest file descriptor
	fdmax = listener; // so far, it's this one

	// main loop
	for(;;)
	{
		//cout<<"entering main loop"<<endl;
		read_fds = master; // copy it
		if (select(fdmax+1, &read_fds, NULL, NULL,NULL) == -1)
		{
			perror("select");
			exit(4);
		}
		//cout<<"listener:"<<listener<<endl;

		// run through the existing connections looking for data to read
		for(i = 0; i <= fdmax; i++)
		{
			//cout<<"run through the existing connections looking for data to read"<<endl;
			if (FD_ISSET(i, &read_fds))
			{
				// we got one!!
				if (i == listener)
				{
					// handle new connections
					addrlen = sizeof remoteaddr;
					newfd = accept(listener,(struct sockaddr *)&remoteaddr,&addrlen);
					if (newfd == -1)
					{
						perror("accept");
					}
					else
					{
						FD_SET(newfd, &master); // add to master set
						if (newfd > fdmax)
						{
							// keep track of the max
							fdmax = newfd;
						}
						printf("select server: new connection from %s on ""socket %d\n",inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr),
							remoteIP, INET6_ADDRSTRLEN),newfd);
					}
				}
				else
				{
					// handle data from a client
					if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0)
					{
						cout<<"nbytes is "<<nbytes<<endl;
						// got error or connection closed by client
						if (nbytes == 0)
						{
							char message[512];
							char buf_send[700];
							// connection closed
							printf("server: socket %d hung up\n", i);
							//cout<<fd_user[i]<<" is offline now"<<fd_user[i]<<endl;
							//unpack(buf+8, "s", msg);
							msg = "";
							msg= fd_user[i] + " is Offline now\0";
						    int attr_length = msg.length() + 4;
						    int length = attr_length + 4;
						    int size = pack(buf_send,"cchhh", (int8_t)packet_recv.version, (int8_t)6,
						    						(int16_t)length,(int16_t)2,(int16_t)attr_length);
							
							msg.copy(message,msg.length(),0);
							size += pack(buf_send+8, "s", message);

						    for(int k = 0; k <= fdmax; k++)
						    {
						    	if(FD_ISSET(k,&master))
						    	{
						    		if(k!=listener && k!=i)
						    		{
						    			if(send(k, buf_send, size, 0) == -1)
						    			{
						    				perror("send");
						    			}
						    		}
						    	}
						    }
						    const string temp=fd_user[i];
						    //set<string>::iterator itset=user_list.find(temp);
						    //user_list.erase(itset);
							map<int,string>::iterator it=fd_user.find(i);
						    fd_user.erase(it);
						    client_count--;
						}
						else
						{
							perror("recv");
						}

						close(i); // bye!
						FD_CLR(i, &master); // remove from master set
					}
					else
					{
							//unpack the incoming packet here
							unpack(buf, "cchhh", &packet_recv.version, &packet_recv.type, &packet_recv.length, &packet_recv.attr.type,&packet_recv.attr.length);

							//act as per the packet
							if(packet_recv.type == 2) //JOIN
							{
								unpack(buf+8, "s", user_name);
								cout<<"JOIN request from "<<user_name<<endl;
								if(client_count==10)
								{
									char message[512];
									char buf_send[700];
									//client connection limit reached so reject new requests with NAK
									msg= " Client connection limit exceeded\0";
									int attr_length = msg.length() + 4;
									int length = attr_length + 4;
									int size = pack(buf_send,"cchhh", (int8_t)packet_recv.version, (int8_t)5,
											(int16_t)length,(int16_t)1,(int16_t)attr_length );

										msg.copy(message,msg.length(),0);
									size += pack(buf_send+8, "s", message);

									if(send(i, buf_send, size, 0) == -1)
									{
										perror("send");
									}
									close(i);
									FD_CLR(i, &master);
								}
								else if(find_map(fd_user,user_name))
								{//user name already exists reject the JOIN
									char message[512];
									char buf_send[700];
									msg= "Username already exists\0";
								    int attr_length = msg.length() + 4;
								    int length = attr_length + 4;
									int size = pack(buf_send,"cchhh", (int8_t)packet_recv.version, (int8_t)5,
											(int16_t)length,(int16_t)1,(int16_t)attr_length );

										msg.copy(message,msg.length(),0);
									size += pack(buf_send+8, "s", message);

									if(send(i, buf_send, size, 0) == -1)
									{
										perror("send");
									}
									//close(i);
									FD_CLR(i, &master);
								}
								else
								{//accept and send ACK
										char message[512];
											char buf_send[700];
									client_count++;
									msg = "";
								    msg = "client count: " + to_string(client_count) + " \nUsernames: ";
								    //msg.copy(message,msg.length(),0);
								    for(map<int, string>::iterator it = fd_user.begin(); it != fd_user.end(); ++it)
								    {
								    	msg = msg + " " + it->second;
											//strcat(message,it->second);
											//strcat(message," ");
								    }
										msg+='\0';
								    //user_list.insert(user_name);
								    fd_user[i]= user_name;
								    int attr_length = msg.length() + 4;
								    int length = attr_length + 4;
								    int size = pack(buf_send,"cchhh", (int8_t)packet_recv.version, (int8_t)7,
								    						(int16_t)length,(int16_t)4,(int16_t)attr_length);
										msg.copy(message,msg.length(),0);
									size += pack(buf_send+8, "s", message);
									
									if(send(i, buf_send, size, 0) == -1)
									{
										perror("send");
									}
									
									char message1[512];
									char buf_send1[700];
									msg = "client " ;
									msg= msg + user_name ;
									msg= msg + " is ONLINE\0";
									attr_length = msg.length() + 4;
									length = attr_length + 4;
									size = pack(buf_send1,"cchhh", (int8_t)packet_recv.version, (int8_t)8,
																	  (int16_t)length,(int16_t)4,(int16_t)attr_length);
								msg.copy(message1,msg.length(),0);
								size += pack(buf_send1 + 8, "s", message1);

								    for(int k = 0; k <= fdmax; k++)
								    {
								    	if(FD_ISSET(k,&master))
								    	{
								    		if(k!=listener && k!=i)
								    		{
								    			if(send(k, buf_send1, size, 0) == -1)
								    			{
								    				perror("send");
								    			}
								    		}
								    	}
								    }
								}

							}

							if(packet_recv.type == 4)
							{
								if(packet_recv.attr.type != 4)
								{
									cout<<"Message not available";
								}
								else
								{
										char message[512];
										char buf_send[700];
									unpack(buf+8, "s", message);
									string str(message);
									msg = "";
									msg= fd_user[i] + ": " + str;
									msg+='\0';
								    int attr_length = msg.length() + 4;
								    int length = attr_length + 4;
								    int size = pack(buf_send,"cchhh", (int8_t)packet_recv.version, (int8_t)3,
								    						(int16_t)length,(int16_t)4,(int16_t)attr_length);
									msg.copy(message,msg.length(),0);
									size += pack(buf_send+8, "s", message);

								    for(int k = 0; k <= fdmax; k++)
								    {
								    	if(FD_ISSET(k,&master))
								    	{
								    		if(k!=listener && k!=i)
								    		{
								    			if(send(k, buf_send, size, 0) == -1)
								    			{
								    				perror("send");
								    			}
								    		}
								    	}
								    }

								}
							}

							if(packet_recv.type == 9)
							{
									char message[512];
									char buf_send[700];
									//unpack(buf+8, "s", msg);
									msg = "";
									msg=fd_user[i];
									msg=msg + " is IDLE\0";
								    int attr_length = msg.length() + 4;
								    int length = attr_length + 4;
								    int size = pack(buf_send,"cchhh", (int8_t)packet_recv.version, (int8_t)9,
								    						(int16_t)length,(int16_t)2,(int16_t)attr_length);
								//cout<<"idle case msg:"<<msg<<endl;
								msg.copy(message,msg.length(),0);
									size += pack(buf_send+8, "s", message);

								    for(int k = 0; k <= fdmax; k++)
								    {
								    	if(FD_ISSET(k,&master))
								    	{
								    		if(k!=listener && k!=i)
								    		{
								    			if(send(k, buf_send, size, 0) == -1)
								    			{
								    				perror("send");
								    			}
								    		}
								    	}
								    }
							}
					}

				} // END handle data from client
			} // END got new incoming connection
		} // END looping through file descriptors
	} // END for(;;)--and you thought it would never end!
	return 0;
}
