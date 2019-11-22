#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <process.h>
#include <vector>
#include <algorithm>
#include <unordered_map>

// link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT 10000 //Use any port you want. Has to be in port forwarding settings.
#define DEFAULT_BUFLEN 512

//Global variables so the other threads can use their information.
std::vector<SOCKET> g_sockets;
std::unordered_map<SOCKET, char*> g_ips;

//Handels one clients data. Recieves data from one client.
//Then Sends it to all the other clients.
DWORD WINAPI ClientSession(LPVOID lpParameter)
{
	//Parameter conversion
	SOCKET ClientSocket = (SOCKET)lpParameter;

	//Recieve and Send Data
	char recvbuf[DEFAULT_BUFLEN];
	int iResult; //Bytes recieved
	int iSendResult;
	int recvbuflen = DEFAULT_BUFLEN;

	// Receive until the peer shuts down the connection
	do
	{
		//Recieve data from the clientSocket.
		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);

		printf("Recieved: %d bytes from: %s \n", iResult, g_ips[ClientSocket]);

		//If there is a result that isnt connection closing, then recieve it and send to all other clients.
		if (iResult > 0)
		{
			//Recieve the message and resize it before sending it back.
			std::string message = recvbuf;
			message.resize(iResult);

			// Send the message to all the other clients active right now.
			for (SOCKET socket : g_sockets)
			{
				//If the socket is valid and not this socket.
				if (socket != SOCKET_ERROR && socket != ClientSocket)
				{
					//Send the message.
					iSendResult = send(socket, message.c_str(), iResult, 0);
					printf("Send: %d bytes to: %s \n", iResult, g_ips[socket]);

					//Error handling.
					if (iSendResult == SOCKET_ERROR)
					{
						printf("Sending data failed. Error code: %d\n", WSAGetLastError());
						break;
					}
				}
			}

		}
		//If connection is closing
		else if (iResult == 0)
			printf("Connection to %s closing...\n", g_ips[ClientSocket]);

	} while (iResult > 0);

	// Shutdown the clientSocket for sending. Because there wont be any data sent anymore.
	iResult = shutdown(ClientSocket, SD_SEND);

	//Error handling.
	if (iResult == SOCKET_ERROR)
	{
		printf("Failed to execute shutdown(). Error Code: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		return 1;
	}

	//Close the socket for good.
	closesocket(ClientSocket);

	//At last remove the socket from the active sockets vector.
	g_sockets.erase(std::remove(g_sockets.begin(), g_sockets.end(), ClientSocket), g_sockets.end());
	return 0;
}

int main()
{
	// Initialize Winsock
	WSADATA wsaData;
	int fResult = 0;

	//Initialize WinSock
	fResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	//Error handling.
	if (fResult != 0)
	{
		printf("WSAStartup failed: %d\n", fResult);
		WSACleanup();
		return 1;
	}

	//Create ListenSocket for clients to connect to.
	SOCKET ListenSocket = INVALID_SOCKET;

	//Create Socket object. 
	ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	//Error handling.
	if (ListenSocket == INVALID_SOCKET) 
	{
		printf("Failed to execute socket(). Error Code: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	sockaddr_in service;
	service.sin_family = AF_INET; //Ipv4 Address
	service.sin_addr.s_addr = INADDR_ANY; //Any IP interface of server is connectable (LAN, public IP, etc).
	service.sin_port = htons(DEFAULT_PORT); //Set port.

	// Bind TCP socket.
	fResult = bind(ListenSocket, reinterpret_cast<SOCKADDR*>(&service), sizeof(SOCKADDR_IN));

	//Error handling.
	if (fResult == SOCKET_ERROR) 
	{
		printf("Failed to execute bind(). Error Code: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	//Start listening to incoming connections.
	//SOMAXCONN is used so the service can define how many inpending connections can be enqueued.
	fResult = listen(ListenSocket, SOMAXCONN);

	//Error handling.
	if (fResult == SOCKET_ERROR)
	{
		printf("Failed to execute listen(). Error Code: %ld\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	//Client Sockets
	SOCKET ClientSocket;

	bool running = true;

	while (running)
	{
		//Reset the client socket.
		ClientSocket = SOCKET_ERROR;

		//Create local variables for the next incoming client.
		SOCKADDR_IN clientAddress;
		int addrlen = sizeof(clientAddress);

		//As long as there is no new client socket
		while (ClientSocket == SOCKET_ERROR)
		{
			//Accept and save the new client socket and fill the local variables with information.
			ClientSocket = accept(ListenSocket, reinterpret_cast<SOCKADDR*>(&clientAddress), &addrlen);
		}

		//Convert the information to a string.
		char* ip = inet_ntoa(clientAddress.sin_addr);
		printf("Connected to: %s \n", ip);

		//Save the socket and ip to the global variables.
		g_sockets.push_back(ClientSocket);
		g_ips[ClientSocket] = ip;

		//Create a new thread for the client and send the client as the argument.
		DWORD dwThreadId;
		CreateThread(NULL, 0, ClientSession, (LPVOID)ClientSocket, 0, &dwThreadId);
	}

	//Close Socket and Cleanup.
	closesocket(ListenSocket);
	WSACleanup();
	return 0;
}