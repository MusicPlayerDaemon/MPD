/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
 * This project's homepage is: http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "interface.h"
#include "buffer2array.h"
#include "command.h"
#include "conf.h"
#include "list.h"
#include "log.h"
#include "listen.h"
#include "sig_handlers.h"
#include "playlist.h"
#include "permission.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define GREETING 		"MPD"

#define INTERFACE_MAX_BUFFER_LENGTH		MAXPATHLEN+1024
#define INTERFACE_LIST_MODE_BEGIN		"command_list_begin"
#define INTERFACE_LIST_MODE_END			"command_list_end"
#define INTERFACE_DEFAULT_OUT_BUFFER_SIZE	4096

int interface_max_connections;
int interface_timeout;
unsigned long long interface_max_command_list_size;
unsigned long long interface_max_output_buffer_size;

typedef struct _Interface {
	char buffer[INTERFACE_MAX_BUFFER_LENGTH+2];
	int bufferLength;
	int fd; /* file descriptor */
	FILE * fp; /* file pointer */
	int open; /* open/used */
	unsigned int permission;
	time_t lastTime;
	List * commandList; /* for when in list mode */
	unsigned long long commandListSize; /* mem commandList consumes */
	List * bufferList; /* for output if client is slow */
	unsigned long long outputBufferSize; /* mem bufferList consumes */
	int expired; /* set whether this interface should be closed on next
			check of old interfaces */
	int num; /* interface number */
	char * outBuffer;
	int outBuflen;
	int outBufSize;
} Interface;

Interface * interfaces = NULL;

void flushInterfaceBuffer(Interface * interface);

void printInterfaceOutBuffer(Interface * interface);

void openInterface(Interface * interface, int fd) {
	int flags;
	
	assert(interface->open==0);

	blockSignals();
	interface->bufferLength = 0;
	interface->fd = fd;
	/* fcntl(interface->fd,F_SETOWN,(int)getpid()); */
	flags = fcntl(fd,F_GETFL);
	flags|=O_NONBLOCK;
	fcntl(interface->fd,F_SETFL,flags);
	interface->fp = fdopen(fd,"rw");
	interface->open = 1;
	interface->lastTime = time(NULL);
	interface->commandList = NULL;
	interface->bufferList = NULL;
	interface->expired = 0;
	interface->outputBufferSize = 0;
	interface->outBuflen = 0;

	interface->permission = getDefaultPermissions();

	interface->outBufSize = INTERFACE_DEFAULT_OUT_BUFFER_SIZE;
#ifdef SO_SNDBUF
	{
		int getSize;
		int sockOptLen = sizeof(int);

		if(getsockopt(interface->fd,SOL_SOCKET,SO_SNDBUF,
					(char *)&getSize,&sockOptLen) < 0)
		{
			DEBUG("problem getting sockets send buffer size\n");
		}
		else if(getSize<=0) {
			DEBUG("sockets send buffer size is not positive\n");
		}
		else interface->outBufSize = getSize;
	}
#endif
	interface->outBuffer = malloc(interface->outBufSize);
	
	unblockSignals();

	myfprintf(interface->fp,"%s %s %s\n",COMMAND_RESPOND_OK,GREETING,
			VERSION);
	printInterfaceOutBuffer(interface);
}

void closeInterface(Interface * interface) {
	assert(interface->open);

	interface->open = 0;

	while(fclose(interface->fp) && errno==EINTR);

	if(interface->commandList) freeList(interface->commandList);
	if(interface->bufferList) freeList(interface->bufferList);

	free(interface->outBuffer);

	SECURE("interface %i: closed\n",interface->num);
}

void openAInterface(int fd, struct sockaddr * addr) {
	int i;

	for(i=0;i<interface_max_connections && interfaces[i].open;i++);

	if(i==interface_max_connections) {
		ERROR("Max Connections Reached!\n");
		while(close(fd) && errno==EINTR);
	}
	else {
		SECURE("interface %i: opened from ",i);
		switch(addr->sa_family) {
		case AF_INET:
			{
				char * host = inet_ntoa(
					((struct sockaddr_in *)addr)->
					sin_addr);
				if(host) { 
					SECURE("%s\n",host);
				}
				else {
					SECURE("error getting ipv4 address\n");
				}
			}
			break;
#ifdef HAVE_IPV6
		case AF_INET6:
			{
				char host[INET6_ADDRSTRLEN+1];
				memset(host,0,INET6_ADDRSTRLEN+1);
				if(inet_ntop(AF_INET6,(void *)
					&(((struct sockaddr_in6 *)addr)->
					sin6_addr),host,INET6_ADDRSTRLEN)) 
				{
					SECURE("%s\n",host);
				}
				else {
					SECURE("error getting ipv6 address\n");
				}
			}
			break;
#endif
		case AF_UNIX:
			SECURE("local connection\n");
			break;
		default:
			SECURE("unknown\n");
		}
		openInterface(&(interfaces[i]),fd);
	}
}

int interfaceReadInput(Interface * interface) {
	blockSignals();
	if(read(interface->fd,interface->buffer+interface->bufferLength,1)>0) {
		int ret = 1;
		int bytesRead = 1;
		while(bytesRead>0) {
			interface->buffer[interface->bufferLength+1] = '\0';
			if(interface->buffer[interface->bufferLength]!='\r') {
				interface->bufferLength++;
			}
			if(interface->bufferLength>=INTERFACE_MAX_BUFFER_LENGTH) {
				break;
			}
			if(interface->buffer[interface->bufferLength-1]=='\n') {
				break;
			}
			bytesRead = read(interface->fd,interface->buffer+
						interface->bufferLength,1);
		}
		unblockSignals();
		if(interface->bufferLength>=INTERFACE_MAX_BUFFER_LENGTH) {
			ERROR("interface %i: buffer overflow\n",
				interface->num);
			closeInterface(interface);
		}
		else if(interface->buffer[interface->bufferLength-1]=='\n') {
			char ** argArray;
			int argArrayLength;

			interface->buffer[interface->bufferLength-1] = '\0';
			interface->bufferLength = 0;
			argArrayLength = buffer2array(interface->buffer,&argArray);

			if(interface->commandList) {
				if(argArrayLength==0);
				else if(strcmp(argArray[0],INTERFACE_LIST_MODE_END)==0) {
					ListNode * node = interface->commandList->firstNode;
					ret = 0;

					while(node!=NULL) {
						char ** argArray;
						int argArrayLength;
						argArrayLength = buffer2array((char *)node->data,&argArray);
						DEBUG("interface %i: process command \"%s\"\n",interface->num,node->data);
						ret = processCommand(interface->fp,&(interface->permission),argArrayLength,argArray);
						DEBUG("interface %i: command returned %i\n",interface->num,ret);
						freeArgArray(argArray,argArrayLength);
						node = node->nextNode;
						if(ret!=0 || 
							interface->expired) {
							node = NULL;
						}
					}
					if(ret==0) {
						myfprintf(interface->fp,"%s\n",COMMAND_RESPOND_OK);
					}
					else if(ret==COMMAND_RETURN_CLOSE ||
							interface->expired) {
						closeInterface(interface);
					}
					printInterfaceOutBuffer(interface);

					freeList(interface->commandList);
					interface->commandList = NULL;
				}
				else {
					interface->commandListSize+=sizeof(ListNode);
					interface->commandListSize+=strlen(interface->buffer)+1;
					if(interface->commandListSize>interface_max_command_list_size) {
						ERROR("interface %i: command list size (%lli) is larger than the max (%lli)\n",interface->num,interface->commandListSize,interface_max_command_list_size);
						closeInterface(interface);
						
					}
					else {
						insertInListWithoutKey(interface->commandList,strdup(interface->buffer));
					}
				}
			}
			else {
				if(argArrayLength &&
						strcmp(argArray[0],
						INTERFACE_LIST_MODE_BEGIN)==0) 
				{
					interface->commandList = makeList(free);
					interface->commandListSize = 
						sizeof(List);
					ret = 1;
				}
				else {
					if(argArrayLength==0) ret = 0;
					else if(strcmp(argArray[0],
							INTERFACE_LIST_MODE_END)
							==0) 
					{
						myfprintf(interface->fp,"%s not in command list mode\n",COMMAND_RESPOND_ERROR);
						ret = -1;
					}
					else {
						DEBUG("interface %i: process command \"%s\"\n",interface->num,interface->buffer);
						ret = processCommand(interface->fp,&(interface->permission),argArrayLength,argArray);
						DEBUG("interface %i: command returned %i\n",interface->num,ret);
					}
					if(ret==0) {
						myfprintf(interface->fp,"%s\n",COMMAND_RESPOND_OK);
					}
					else if(ret==COMMAND_RETURN_CLOSE ||
							interface->expired) {
						closeInterface(interface);
					}
					printInterfaceOutBuffer(interface);
				}
			}
			freeArgArray(argArray,argArrayLength);
		}
		return ret;
	}
	else {
		unblockSignals();
		closeInterface(interface);
	}

	return 1;
}

void addInterfacesReadyToReadAndListenSocketToFdSet(fd_set * fds, int * fdmax) {
	int i;

	FD_ZERO(fds);
	FD_SET(listenSocket,fds);
	if(*fdmax<listenSocket) *fdmax = listenSocket;

	for(i=0;i<interface_max_connections;i++) {
		if(interfaces[i].open && !interfaces[i].expired && !interfaces[i].bufferList) {
			FD_SET(interfaces[i].fd,fds);
			if(*fdmax<interfaces[i].fd) *fdmax = interfaces[i].fd;
		}
	}
}

void addInterfacesForBufferFlushToFdSet(fd_set * fds, int * fdmax) {
	int i;

	FD_ZERO(fds);

	for(i=0;i<interface_max_connections;i++) {
		if(interfaces[i].open && !interfaces[i].expired && interfaces[i].bufferList) {
			FD_SET(interfaces[i].fd,fds);
			if(*fdmax<interfaces[i].fd) *fdmax = interfaces[i].fd;
		}
	}
}

void closeNextErroredInterface() {
	fd_set fds;
	struct timeval tv;
	int i;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	for(i=0;i<interface_max_connections;i++) {
		if(interfaces[i].open) {
			FD_ZERO(&fds);
			FD_SET(interfaces[i].fd,&fds);
			if(select(FD_SETSIZE,&fds,NULL,NULL,&tv)<0) {
				closeInterface(&interfaces[i]);
				return;
			}
		}
	}
}

int doIOForInterfaces() {
	fd_set rfds;
	fd_set wfds;
	struct timeval tv;
	int i;
	int selret;
	int fdmax = 0;

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	addInterfacesReadyToReadAndListenSocketToFdSet(&rfds,&fdmax);
	addInterfacesForBufferFlushToFdSet(&wfds,&fdmax);

	while((selret = select(fdmax+1,&rfds,&wfds,NULL,&tv))) {
		if(FD_ISSET(listenSocket,&rfds)) getConnections(listenSocket);
		if(selret<0 && errno==EINTR) break;
		else if(selret<0) {
			closeNextErroredInterface();
			continue;
		}
		for(i=0;i<interface_max_connections;i++) {
			if(interfaces[i].open && FD_ISSET(interfaces[i].fd,&rfds)) {
				if(COMMAND_RETURN_KILL==interfaceReadInput(&(interfaces[i]))) {
					return COMMAND_RETURN_KILL;
				}
				interfaces[i].lastTime = time(NULL);
			}
			if(interfaces[i].open && FD_ISSET(interfaces[i].fd,&wfds)) {
				flushInterfaceBuffer(&interfaces[i]);
				interfaces[i].lastTime = time(NULL);
			}
		}
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		fdmax = 0;
		addInterfacesReadyToReadAndListenSocketToFdSet(&rfds,&fdmax);
		addInterfacesForBufferFlushToFdSet(&wfds,&fdmax);
	}

	return 1;
}

void initInterfaces() {
	int i;
	char * test;

	interface_timeout = strtol((getConf())[CONF_CONNECTION_TIMEOUT],&test,10);
	if(*test!='\0' || interface_timeout<=0) {
		ERROR("connection timeout \"%s\" is not a positive integer\n",(getConf())[CONF_CONNECTION_TIMEOUT]);
		exit(-1);
	}

	interface_max_connections = strtol((getConf())[CONF_MAX_CONNECTIONS],&test,10);
	if(*test!='\0' || interface_max_connections<=0) {
		ERROR("max connections \"%s\" is not a positive integer\n",(getConf())[CONF_MAX_CONNECTIONS]);
		exit(-1);
	}

	interface_max_command_list_size = strtoll((getConf())[CONF_MAX_COMMAND_LIST_SIZE],&test,10);
	if(*test!='\0' || interface_max_command_list_size<=0) {
		ERROR("max command list size \"%s\" is not a positive integer\n",(getConf())[CONF_MAX_COMMAND_LIST_SIZE]);
		exit(-1);
	}

	interface_max_output_buffer_size = strtoll((getConf())[CONF_MAX_OUTPUT_BUFFER_SIZE],&test,10);
	if(*test!='\0' || interface_max_output_buffer_size<=0) {
		ERROR("max output buffer size \"%s\" is not a positive integer\n",(getConf())[CONF_MAX_OUTPUT_BUFFER_SIZE]);
		exit(-1);
	}

	interface_max_command_list_size*=1024;
	interface_max_output_buffer_size*=1024;

	interfaces = malloc(sizeof(Interface)*interface_max_connections);

	for(i=0;i<interface_max_connections;i++) {
		interfaces[i].open = 0;
		interfaces[i].num = i;
	}
}

void closeAllInterfaces() {
	int i;

	fflush(NULL);

	for(i=0;i<interface_max_connections;i++) {
		if(interfaces[i].open) {
			closeInterface(&(interfaces[i]));
		}
	}
}

void freeAllInterfaces() {
	closeAllInterfaces();

	free(interfaces);
}

void closeOldInterfaces() {
	int i;

	for(i=0;i<interface_max_connections;i++) {
		if(interfaces[i].open && (interfaces[i].expired || (time(NULL)-interfaces[i].lastTime>interface_timeout))) {
			DEBUG("interface %i: timeout\n",i);
			closeInterface(&(interfaces[i]));
		}
	}
}

void closeInterfaceWithFD(int fd) {
	int i;

	for(i=0;i<interface_max_connections;i++) {
		if(interfaces[i].fd==fd) {
			closeInterface(&(interfaces[i]));
		}
	}
}

void flushInterfaceBuffer(Interface * interface) {
	ListNode * node = NULL;
	char * str;
	int ret = 0;

	while((node = interface->bufferList->firstNode)) {
		str = (char *)node->data;
		if((ret = write(interface->fd,str,strlen(str)))<0) break;
		else if(ret<strlen(str)) {
			interface->outputBufferSize-=ret;
			str = strdup(&str[ret]);
			free(node->data);
			node->data = str;
		}
		else {
			interface->outputBufferSize-= strlen(str)+1;
			interface->outputBufferSize-= sizeof(ListNode);
			deleteNodeFromList(interface->bufferList,node);
		}
		interface->lastTime = time(NULL);
	}

	if(!interface->bufferList->firstNode) {
		DEBUG("interface %i: buffer empty\n",interface->num);
		freeList(interface->bufferList);
		interface->bufferList = NULL;
	}
	else if(ret<0 && errno!=EAGAIN && errno!=EINTR) {
		/* cause interface to close */
		DEBUG("interface %i: problems flushing buffer\n",
			interface->num);
		freeList(interface->bufferList);
		interface->bufferList = NULL;
		interface->expired = 1;
	}
}

void flushAllInterfaceBuffers() {
	int i;

	for(i=0;i<interface_max_connections;i++) {
		if(interfaces[i].open && !interfaces[i].expired && interfaces[i].bufferList) {
			flushInterfaceBuffer(&interfaces[i]);
		}
	}
}

int interfacePrintWithFD(int fd,char * buffer) {
	int i;
	int buflen;
	int copylen;
	Interface * interface;

	if(!(buflen = strlen(buffer))) return -1;

	for(i=0;i<interface_max_connections;i++) {
		if(interfaces[i].open && interfaces[i].fd==fd) break;
	}

	/* if fd isn't found or interfaces is going to be closed, do nothing */
	if(i==interface_max_connections) return -1;
	if(interfaces[i].expired) return 0;

	interface = interfaces+i;

	while(buflen>0) {
		copylen = buflen>
			interface->outBufSize-interface->outBuflen?
			interface->outBufSize-interface->outBuflen:
			buflen;
		memcpy(interface->outBuffer+interface->outBuflen,buffer,
			copylen);
		buflen-=copylen;
		interface->outBuflen+=copylen;
		buffer+=copylen;
		if(interface->outBuflen>=interface->outBufSize) {
			printInterfaceOutBuffer(interface);
		}
	}

	return 0;
}

void printInterfaceOutBuffer(Interface * interface) {
	char * buffer;
	int ret;

	if(!interface->open || interface->expired || !interface->outBuflen) {
		return;
	}

	if(interface->bufferList) {
		interface->outputBufferSize+=sizeof(ListNode);
		interface->outputBufferSize+=interface->outBuflen+1;
		if(interface->outputBufferSize>
				interface_max_output_buffer_size)
 		{
			ERROR("interface %i: output buffer size (%lli) is "
					"larger than the max (%lli)\n",
					interface->num,
					interface->outputBufferSize,
					interface_max_output_buffer_size);
			/* cause interface to close */
			freeList(interface->bufferList);
			interface->bufferList = NULL;
			interface->expired = 1;
		}
		else {
			buffer = malloc(interface->outBuflen+1);
			memcpy(buffer,interface->outBuffer,interface->outBuflen);
			buffer[interface->outBuflen] = '\0';
			insertInListWithoutKey(interface->bufferList,(void *)buffer);
			flushInterfaceBuffer(interface);
		}
	}
	else {
		if((ret = write(interface->fd,interface->outBuffer,
				interface->outBuflen))<0) 
		{
			if(errno==EAGAIN || errno==EINTR) {
				buffer = malloc(interface->outBuflen+1);
				memcpy(buffer,interface->outBuffer,
						interface->outBuflen);
				buffer[interface->outBuflen] = '\0';
				interface->bufferList = makeList(free);
				insertInListWithoutKey(interface->bufferList,
						(void *)buffer);
			}
			else {
				DEBUG("interface %i: problems writing\n",
						interface->num);
				interface->expired = 1;
				return;
			}
		}
		else if(ret<interface->outBuflen) {
			buffer = malloc(interface->outBuflen-ret+1);
			memcpy(buffer,interface->outBuffer+ret,
					interface->outBuflen-ret);
			buffer[interface->outBuflen-ret] = '\0';
			interface->bufferList = makeList(free);
			insertInListWithoutKey(interface->bufferList,buffer);
		}
		/* if we needed to create buffer, initialize bufferSize info */
		if(interface->bufferList) {
			DEBUG("interface %i: buffer created\n",interface->num);
			interface->outputBufferSize = sizeof(List);
			interface->outputBufferSize+=sizeof(ListNode);
			interface->outputBufferSize+=strlen(
					(char *)interface->bufferList->
					firstNode->data)+1;
		}
	}

	interface->outBuflen = 0;
}
