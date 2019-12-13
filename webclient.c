/*
 *
 *  Created on: Dec 12,2019 
 *      Author: Komal
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <netdb.h>

#define PORT			"http"
#define PATH_SIZE		60
#define HOST_SIZE		30
#define SEND_MSG_SIZE 	13312
#define BUFFER_SIZE 	13312 /* 1024 *13 */
#define IP_LEN 			16
#define PORT_LEN 		10
#define DEBUG

int extractURL(char *url, char *host, char *path);
int getAddrInfo(char *host, struct addrinfo **serv_info);
int findClientInfo(char *client_ip, char *portnumber, int sock_fd);
void createSendMsg(char *send_msg, char *url, char *client_ip, char *portnumber);

int main(int argc, char *argv[])
{
	int addr_info, sock_fd, sock_conn, sock_read, sock_write, sock_shut, sock_close, client_info;
	char path[PATH_SIZE], host[HOST_SIZE];
	struct addrinfo hints, *serv_info, *rp;
	struct sockaddr_in sock_addr;
	char send_msg[SEND_MSG_SIZE];
	char portnumber[PORT_LEN];
	char client_ip[IP_LEN];

	int ret			= 0;
	char *buffer	= malloc(sizeof(char) * BUFFER_SIZE);
	char *recv_msg	= buffer;
	int buffer_len	= BUFFER_SIZE;

	if (argc != 2)
	{
		printf("Number of arguments should be 2\n");
		return -1;
	}
	ret = extractURL(argv[1], host, path);
	if (ret == 0)
	{
		printf("Error (extractURL): url extraction not successful\n");
		return -1;
	}
	/* Get server info */
	addr_info = getAddrInfo(host, &serv_info);
	if (addr_info != 0)
	{
		printf("Error (getaddrinfo): %s\n", gai_strerror(addr_info));
		return -1;
	}
	/* create a client socket and connect the socket from the list of addrinfo*/
	for (rp = serv_info; rp != NULL; rp = rp->ai_next)
	{
		sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock_fd < 0)
			continue;
		DEBUG("Socket created\n");
		sock_conn = connect(sock_fd, rp->ai_addr, rp->ai_addrlen);
		if (sock_conn != -1)
			break;
		close(sock_fd);
	}
	if (rp == NULL)
	{
		printf("No address in the list was a success\n");
		return -1;
	}
	freeaddrinfo(serv_info);
	DEBUG("Socket connected \n");
	/* finding the IP address and port number of the client */
	client_info = findClientInfo(client_ip, portnumber, sock_fd);
	if (client_info < 0)
	{
		printf("Could not find client information\n");
		return -1;
	}
	/* create the http request to send to the server */
	createSendMsg(send_msg, argv[1], client_ip, portnumber);
	DEBUG("Final msg to send:\n%s\n",send_msg);
	/* Send the http request to server */
	sock_write = write(sock_fd, send_msg, strlen(send_msg));
	if (sock_write < 0)
	{
		printf("Error (write): %s\n", strerror(errno));
		return -1;
	}
	DEBUG("Socket write complete\n");
	/* Shut down the socket for writes*/
	sock_shut = shutdown(sock_fd, SHUT_WR);
	if (sock_shut < 0)
	{
		printf("Error (shutdown): %s\n", strerror(errno));
		return -1;
	}
	DEBUG("Socket shutdown \n");
	/* Since response is bitstream, read the response until eof */
	while(1)
	{
		/* Read response and store it in buffer */
		sock_read = read(sock_fd, buffer, buffer_len);
		if (sock_read < 0)
		{
			printf("Error (read): %s\n", strerror(errno));
			return -1;
		}
		buffer		+= sock_read;
		buffer_len	-= sock_read;
		/* if eof is reached, break out of loop */
		if (sock_read == 0)
			break;
	}
	buffer++;
	*buffer = '\0';
	DEBUG("Socket read complete\n");
	/* Print the http response */
	printf("%s", recv_msg);
	/* Close the socket */
	sock_close = close(sock_fd);
	if (sock_close < 0)
	{
		printf("Error (close): %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

/**
 * extractURL() - extract the host and path from URL
 * url: full url
 * host: to store the host portion of url
 * path: to store the path portion of url
 * return status of conversion
 */ 
int extractURL(char *url, char *host, char *path)
{
	int ret = 0;
	if (sscanf(url, "http://%99[^/]/%199[^\n]", host, path) == 2)
		ret = 1;/* http://hostname/page	*/
	else if (sscanf(url, "http://%99[^/]/[^\n]", host) == 1)
		ret = 1;  /* http://hostname/ */
	else if (sscanf(url, "http://%99[^\n]", host) == 1)
		ret = 1;  /* http://hostname */
	return ret;
}

/**
 * createSendMsg() - create http request
 * send_msg: to store http response message
 * url: url input
 * client_ip: ip address of client
 * portnumber: portnumber of client socket
 */ 
void createSendMsg(char *send_msg, char *url, char *client_ip, char *portnumber)
{
	strcpy(send_msg, "POST ");
	strcat(send_msg, url);
	strcat(send_msg, " HTTP/1.0\r\n");
	strcat(send_msg, "\r\n");
	strcat(send_msg, "ClientIP = ");
	strcat(send_msg, client_ip);
	strcat(send_msg, "\r\n");
	strcat(send_msg, "ClientPort = ");
	strcat(send_msg, portnumber);
	strcat(send_msg, "\r\n");
	return;
}

/**
 * getAddrInfo() - get addr info of server
 * host: server host information
 * serv_info: to store server information
 * return addinfo status
 */
int getAddrInfo(char *host, struct addrinfo **serv_info)
{
	struct addrinfo hints;
	int addr_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family		= AF_INET;
	hints.ai_socktype	= SOCK_STREAM;
	hints.ai_protocol	= 0;
	hints.ai_flags		= 0;
	addr_info		= getaddrinfo(host, PORT, &hints, serv_info);
	return addr_info;
}

/**
 * findClientInfo() - get info of client
 * client_ip: to store client ip address
 * portnumber: to store client port number
 * sock_fd: file descriptor of socket
 * return status
 */
int findClientInfo(char *client_ip, char *portnumber, int sock_fd)
{
	struct sockaddr_in client_addr;
	int port, client_info, client_len;
	const char *res;

	client_len = sizeof(client_addr);
	memset(&client_addr, 0, sizeof(client_addr));
	client_info = getsockname(sock_fd, (struct sockaddr *)&client_addr, &client_len);
	if (client_info < 0)
	{
		printf("Error (getsockname): %s\n", strerror(errno));
		return -1;
	}
	/* converting binary IP address into text */
	res = inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, IP_LEN);
	if (!res)
	{
		printf("Error (inet_ntop): %s\n", strerror(errno));
		return -1;
	}
	/* convert port number from network byte order to host byte order */
	port = ntohs(client_addr.sin_port);
	/* converting port number to a string */
	sprintf(portnumber, "%d", port);
	return 0;
}
