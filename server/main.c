#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

// According to POSIX.1-2001
#include <sys/select.h>

// According to earlier standards
#include <sys/time.h>
#include <sys/types.h>	// fd_set
#include <unistd.h>

//
#define TRUE 1
#define FALSE 0

// https://www.ibm.com/docs/en/i/7.1?topic=designs-example-nonblocking-io-select

// void FD_CLR  (int fd, fd_set *set) - Removes socket s from set
// int  FD_ISSET(int fd, fd_set *set) - Checks to see if s is a member of set and returns TRUE if so
// void FD_SET  (int fd, fd_set *set) - Adds socket s to set
// void FD_ZERO (        fd_set *set) - Initializes set to the empty set. A set should always be cleared before using

int handleNewClients(int serverSocketDescriptor, fd_set* fdMasterSet, int* maxSD);
int handleIncomingDataForClient();

int main(int argc, char* argv[]) {
	// create an AF_INET6 stream socket
	int serverSocketDescriptor = socket(AF_INET6, SOCK_STREAM, 0);
	if (serverSocketDescriptor < 0) {
		printf("SERVER/ERROR: socket() failed");
		exit(-1);
	}

	// allow socket descriptor to be reuseable
	int on = 1;
	int err = setsockopt(serverSocketDescriptor, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
	if (err < 0) {
		printf("SERVER/ERROR: setsockopt() failed");
		close(serverSocketDescriptor);
		exit(-1);
	}

	// set nonblocking, all incoming connections will inherit this
	err = ioctl(serverSocketDescriptor, FIONBIO, (char*)&on);
	if (err < 0) {
		printf("SERVER/ERROR: ioctl() failed");
		close(serverSocketDescriptor);
		exit(-1);
	}

	// bind the socket
	struct sockaddr_in6 addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(8888);
	memcpy(&addr.sin6_addr, &in6addr_any, sizeof(in6addr_any));

	err = bind(serverSocketDescriptor, (struct sockaddr*)&addr, sizeof(struct sockaddr_in6));
	if (err < 0) {
		printf("SERVER/ERROR: bind() failed");
		close(serverSocketDescriptor);
		exit(-1);
	}

	// set the listen back log
	err = listen(serverSocketDescriptor, 32);  // 32 requests to connect will be queued before refusing
	if (err < 0) {
		printf("SERVER/ERROR: listen() failed");
		close(serverSocketDescriptor);
		exit(-1);
	}

	// init
	int maxSD = serverSocketDescriptor;

	fd_set fdWorkingSet;

	fd_set fdMasterSet;
	FD_ZERO(&fdMasterSet);
	FD_SET(serverSocketDescriptor, &fdMasterSet);

	struct timeval timeout;
	timeout.tv_sec = 3 * 60;  // 3 min
	timeout.tv_usec = 0;	  // 1000000;	// 1/2 second; 1,000,000 microseconds is 1 second

	int terminateServer = FALSE;

	// waiting for incoming connects or for incoming data
	do {
		memcpy(&fdWorkingSet, &fdMasterSet, sizeof(fdMasterSet));  // clone master set

		printf("SERVER/select\n");
		err = select(maxSD + 1, &fdWorkingSet, NULL, NULL, &timeout);  // call select() and wait the configured timeout

		if (err < 0) {	// check if select call failed
			printf("SERVER/select: select() failed");
			break;
		}

		if (err == 0) {	 // check to see timed out
			printf("SERVER/select: select() %lu microseconds timed out\n", timeout.tv_usec);
			continue;
		}

		int nrOfDescriptorsReady = err;
		printf("SERVER/select: got %d sockets ready for use\n", nrOfDescriptorsReady);

		// one or more descriptors are readable, find which ones
		for (int i = 0; i <= maxSD && nrOfDescriptorsReady > 0; ++i) {
			// check if descriptor ready
			if (FD_ISSET(i, &fdWorkingSet)) {
				nrOfDescriptorsReady -= 1;

				// check if this is the listening server socket, if yes => new clients
				if (i == serverSocketDescriptor) {
					printf("SERVER/select: new clients on %d\n", serverSocketDescriptor);

					terminateServer = handleNewClients(serverSocketDescriptor, &fdMasterSet, &maxSD);
					if (terminateServer) {
						break;
					}
				}

				// => incoming data on existing connection
				else {
					printf("SERVER/select: new data on %d\n", i);

					// receive all incoming data on this socket
					// then loop back and call select again
					int closeConnection = handleIncomingDataForClient(i);
					if (closeConnection) {
						close(i);
						FD_CLR(i, &fdMasterSet);
						if (i == maxSD) {
							while (FD_ISSET(maxSD, &fdMasterSet) == FALSE) {
								maxSD -= 1;
							}
						}
					}
				}
			}
		}

	} while (!terminateServer);

	// clean up
	for (int i = 0; i <= maxSD; ++i) {
		if (FD_ISSET(i, &fdMasterSet)) {
			close(i);
		}
	}
}

int handleNewClients(int serverSocketDescriptor, fd_set* fdMasterSet, int* maxSD) {
	// accept all incoming connections queued up on the listening socket
	// then loop back and call select again
	int clientSocketDescriptor;
	do {
		// accept each incoming connection
		// if accept fails with EWOULDBLOCK => we have accepted all of them
		// any other failure on accept will cause us to end the server
		int newSocketDescriptor = accept(serverSocketDescriptor, NULL, NULL);
		if (newSocketDescriptor < 0) {
			if (errno != EWOULDBLOCK) {
				printf("SERVER: accept() failed");
				return TRUE;  // stop server
			}
			break;
		}

		// add the new incoming connection to the master set
		printf("SERVER/accept: new client - %d\n", newSocketDescriptor);
		FD_SET(newSocketDescriptor, fdMasterSet);
		if (newSocketDescriptor > *maxSD) {
			*maxSD = newSocketDescriptor;
		}
	} while (clientSocketDescriptor != -1);

	return FALSE;  // don't stop server
}

int handleIncomingDataForClient(int clientSocketDescriptor) {
	char buffer[80];
	do {
		// RECV data on this connection until EWOULDBLOCK
		// mark for close otherwise
		memset(buffer, 0, 80);
		int err = recv(clientSocketDescriptor, buffer, sizeof(buffer), 0);
		if (err < 0) {
			if (errno != EWOULDBLOCK) {
				printf("SERVER/handleIncomingDataForClient: recv() failed");
				return TRUE;  // close connection
			}
			break;
		}

		// check if connection has been closed by the client =>  mark for close
		if (err == 0) {
			printf("SERVER/handleIncomingDataForClient: client closed connection\n");
			return TRUE;  // close connection
		}

		// data was received, send PONG
		int len = err;
		printf("SERVER/handleIncomingDataForClient: %d bytes received: %s\n", len, buffer);

		err = send(clientSocketDescriptor, "PONG", len, 0);
		if (err < 0) {
			printf("SERVER/handleIncomingDataForClient: send() failed");
			return TRUE;  // close connection
		}

	} while (TRUE);
}
