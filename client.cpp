/*
 * client.cpp
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

int main(int argc, char* argv[])
{

	if(argc!=4)
	{
		cout<<"please enter client, username, server_ip_address, server_port"<<endl;
	}

	fd_set read_fds; // temp file descriptor list for select()
	struct SBCP_packet packet_recv, packet_send;
	char user_name[16];
	char message[512];
	string msg="";
	char buf[700]; // buffer for incoming data
	char buf_send1[700]; //buffer for outgoing data
	int socket_fd, nbytes;
	int i, j, rv;
	struct addrinfo hints, *ai, *p;

	struct timeval timeout, start, end;
	timeout.tv_sec = 10;
	timeout.tv_usec = 00000;


	FD_ZERO(&read_fds);

	// get us a socket and bind it
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(argv[2], argv[3], &hints, &ai)) != 0) {
		fprintf(stderr, "select of server: %s\n", gai_strerror(rv));
		exit(1);
	}

	for(p = ai; p != NULL; p = p->ai_next)
	{
		socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (socket_fd == -1)
		{
			perror("client: socket");
			continue;
		}

		if (connect(socket_fd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(socket_fd);
			perror("client: connect");
			continue;
		}
		break;

	}

	// if we got here, it means we didn't get bound
	if (p == NULL)
	{
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	freeaddrinfo(ai); // all done with this

	strcpy(user_name,argv[1]);


	int attr_length = strlen(user_name) + 4;
	int length = attr_length + 4;
	//prepare data of username
	int packetsize = pack(buf_send1, "cchhhs", (int8_t)3, (int8_t)2,
		(int16_t)length, (int16_t)2, (int16_t)attr_length, user_name);

	if (send(socket_fd, buf_send1, packetsize, 0) == -1)
	{
		perror("send");
		exit(1);
	}

	// add the keyboard console to the read_fd set
	FD_SET(0, &read_fds);
	// add the socket to the read_fd set
    FD_SET(socket_fd, &read_fds);

	// main loop
	while (1)
	{

		gettimeofday(&start, NULL);

		if (select(socket_fd + 1, &read_fds, NULL, NULL, &timeout) == -1)
		{
			perror("select");
			exit(4);
		}

		gettimeofday(&end, NULL);

		double interval = end.tv_sec - start.tv_sec;
		
		if (interval >= 10)
		{
			char buf_send[700];
			cout << "Timeout\n";
			packetsize = pack(buf_send, "cchhh", (int8_t)3, (int8_t)9,
				(int16_t)length, (int16_t)1, (int16_t)attr_length);

			// send idle message to server
			if (send(socket_fd, buf_send, packetsize, 0) == -1) {
				perror("send");
				exit(1);
			}

		}

		for (i = 0; i <= socket_fd; i++) {// find if there is data to read from server or keyboard


			if (FD_ISSET(i, &read_fds)) {

				if (i == 0) {//there is data to read from keyboard

					char buf_input[512];
					char buf_send[700];
					fgets(buf_input, sizeof(buf_input), stdin);
					int len = strlen(buf_input) - 1;

					if (buf_input[len] == '\n')
						buf_input[len] = '\0';
					attr_length = len + 4;
					length = attr_length + 4;
					// chage data to network byte order
					packetsize = pack(buf_send, "cchhhs", (int8_t)3, (int8_t)4,
						(int16_t)length, (int16_t)4, (int16_t)attr_length, buf_input);

					// send chat message to server
					if (send(socket_fd, buf_send, packetsize, 0) == -1) {
						perror("send");
						exit(1);
					}
				}

				if (i == socket_fd) 
				{//there is data to read from server
								  // receive chat message to server
					if ((nbytes = recv(socket_fd, buf, sizeof buf, 0)) <= 0) 
					{
						//perror("recv");
						exit(1);
					}
					else
					{

						unpack(buf, "cchhh", &packet_recv.version, &packet_recv.type, &packet_recv.length, &packet_recv.attr.type,
							       &packet_recv.attr.length);

						unpack(buf + 8, "s", message);
						message[nbytes-8] = '\0';

						if (packet_recv.type == 7) //ACK
						{
							cout << "connection accepted:ACK \n";
							cout <<endl<< message << endl;
						}
						else if (packet_recv.type == 5) //NAK
						{
							cout << "connection rejected NAK\n";
							cout << endl<<message<<endl;
							exit(1);
						}
						else if (packet_recv.type == 8 || packet_recv.type == 6 || packet_recv.type == 9) //ONLINE, OFFLINE, IDLE same handling
						{
							//cout << "ONLINE OFFLINE IDLE \n";
							cout << endl<<message << endl;
						}
						else if (packet_recv.type == 3) //FWD
						{
							//cout << "chat received \n";
							cout << endl<<message << endl;
						}

					}
				
				}

			}

			FD_SET(0, &read_fds);
			FD_SET(socket_fd, &read_fds);
		}


    }

	    close(socket_fd);
		return 0;
}




