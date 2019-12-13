/*
 * 
 *
 *  Created on: Dec 12, 2019
 *      Author: Komal
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#define PORT			80
#define QUEUE_LEN		10
#define IP_LEN			16
#define PORT_LEN		10
#define SEND_MSG_SIZE	13312
#define BUFFER_SIZE		13312
#define DEBUG

void signalhandler(int signal);
void createHtmlmsg(char *recv_msg, char *html);
void printChildInfo(char *client_ip, char *portnumber);
void handleHttpClient(int sock_fd, struct sockaddr_in client_addr);
int findClientInfo(char *client_ip, char *portnumber, struct sockaddr_in client_addr);
void createResponse(int sock_fd, char *send_msg, char *recv_msg, char *client_ip, char *portnumber);

int main(int argc, char *argv[])
{
	int master_fd, sock_bind, sock_listen, slave_fd, sock_close;
	struct sockaddr_in serv_addr, client_addr;
	int client_len = sizeof(client_addr);
	int pid;

	master_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (master_fd < 0)
	{
		printf("Error (socket): %s\n", strerror(errno));
		return -1;
	}
	DEBUG("socket()\n");
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_port			= htons(PORT);
	serv_addr.sin_addr.s_addr	= htonl(INADDR_ANY);
	sock_bind = bind(master_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (sock_bind < 0)
	{
		printf("Error (bind): %s\n", strerror(errno));
		return -1;
	}
	DEBUG("bind()\n");
	sock_listen = listen(master_fd, QUEUE_LEN);
	if (sock_listen < 0)
	{
		printf("Error (listen): %s\n", strerror(errno));
		return -1;
	}
	DEBUG("listen()\n");
	(void) signal(SIGCHLD, signalhandler);
	memset(&client_addr, 0, sizeof(client_addr));
	while (1)
	{
		slave_fd = accept(master_fd, (struct sockaddr *)&client_addr, &client_len);
		if (slave_fd < 0)
		{
			if (errno == EINTR)
				continue;
			printf("Error (accept): %s\n", strerror(errno));
			return -1;
		}
		DEBUG("accept()\n");
		pid = fork();
		switch (pid)
		{
			case -1:/* fork error */
					printf("Error (fork): %s\n", strerror(errno));
					return -1;
			case 0:/* child process */
					sock_close = close(master_fd);
					if (sock_close < 0)
					{
						printf("Error (master close): %s\n", strerror(errno));
						exit(-1);
					}
					handleHttpClient(slave_fd, client_addr);
					sock_close = close(slave_fd);
					if (sock_close < 0)
					{
						printf("Error (slave close): %s\n", strerror(errno));
						exit(-1);
					}
					exit(0);
					break;
			default: /* parent process */
					sock_close = close(slave_fd);
					if (sock_close < 0)
					{
						printf("Error (slave close): %s\n", strerror(errno));
						return -1;
					}
					break;
		}
	}
	return 0;
}

/**
 * signalhandler() - to handle child process
 * signal: SIGCHLD
 */
void signalhandler(int signal)
{
	int status, child_pid;

	child_pid = wait3(&status, WNOHANG, (struct rusage *)0);
	while (child_pid >= 0)
	{
		if (status < 0)
			printf("child error exit, pid = %d\n", child_pid);
	}
	return;
}

/**
 * handleHttpClient() - to handle each client
 * sock_fd: socket file descriptor
 * client_addr: client address
 */
void handleHttpClient(int sock_fd, struct sockaddr_in client_addr)
{
	int sock_read, sock_write, client_info;
	char send_msg[SEND_MSG_SIZE];
	char client_ip[IP_LEN], portnumber[PORT_LEN];
	char *buffer	= malloc(BUFFER_SIZE * sizeof(char));
	char *rcvd_msg	= buffer;
	int buffer_len	= BUFFER_SIZE;

	client_info = findClientInfo(client_ip, portnumber, client_addr);
	if (client_info < 0)
	{
		printf("Could not extract client info\n");
		exit(-1);
	}
	printChildInfo(client_ip, portnumber);
	while (1)
	{
		sock_read = read(sock_fd, buffer, buffer_len);
		if (sock_read < 0)
		{
			printf("Error (read): %s\n", strerror(errno));
			exit(-1);
		}
		buffer		+= sock_read;
		buffer_len	-= sock_read;
		if (sock_read == 0)
			break;
	}
	buffer++;
	*buffer = '\0';
	createResponse(sock_fd, send_msg, rcvd_msg, client_ip, portnumber);
	sock_write = send(sock_fd, send_msg, strlen(send_msg), 0);
	if (sock_write < 0)
	{
		printf("Error (send): %s\n", strerror(errno));
		exit(-1);
	}
	sleep(10);
	return;
}

/**
 * createResponse() - create http response
 * sock_fd: socket file descriptor
 * send_msg: to store http response
 * recv_msg: http request
 * client_ip: client ip address
 * portnumber: client port number'
 */
void createResponse(int sock_fd, char *send_msg, char *recv_msg, char *client_ip, char *portnumber)
{
	char html_msg[200];

	createHtmlmsg(recv_msg, html_msg);
	strcpy(send_msg, "HTTP/1.0 200 OK\r\n");
	strcat(send_msg, "ClientIP: ");
	strcat(send_msg, client_ip);
	strcat(send_msg, "\r\n");
	strcat(send_msg, "ClientPort: ");
	strcat(send_msg, portnumber);
	strcat(send_msg, "\r\n\r\n");
	strcat(send_msg, html_msg);
	strcat(send_msg, "\r\n\r\n");
	return;
}

/**
 * createHtmlmsg() - create html message of http response
 * recv_msg: http request
 * html: to store html message
 */
void createHtmlmsg(char *recv_msg, char *html)
{
	strcpy(html, "<!DOCTYPE html>\n");
	strcat(html, "<html><body><blockquote>\n");
	strcat(html, recv_msg);
	strcat(html, "\n");
	strcat(html, "</blockquote></body></html>\n");
	return;
}

/**
 * findClientInfo() - find client information
 * client_ip: to store client ip address
 * portnumber: to store clietn port number
 * client_addr: client address information
 * return status
 */
int findClientInfo(char *client_ip, char *portnumber, struct sockaddr_in client_addr)
{
	const char *res;
	int port;

	res = inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, IP_LEN);
	if (!res)
	{
		printf("Error (inet_ntop): %s\n", strerror(errno));
		exit(-1);
	}
	port = htons(client_addr.sin_port);
	sprintf(portnumber, "%d", port);
	return 0;
}

/**
 * printChildInfo() - print each child process and client information
 * client-ip: ip address of the client
 * portnumber: port number of client
 */
void printChildInfo(char *client_ip, char *portnumber)
{
	int my_pid = getpid();
	
	printf("server-pid = %d, ", my_pid);
	printf("client-ip = %s, ", client_ip);
	printf("client-port = %s\n", portnumber);
	return;
}

