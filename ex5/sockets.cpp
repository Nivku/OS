#include <iostream>
#include <string>
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


/// CONSTANTS ///
#define BUFFER 256
#define SERVER "server"
#define CLIENT "client"
#define Five 5

/// ERROR MESSAGES ///
#define HOST_NAME_FAILURE "system error: gethostname function failure"
#define SOCKET_FAILURE "system error: socket function failure"
#define BIND_FAILURE "system error: Server bind function failure"
#define LISTEN_FAILURE "system error: Server listen function failure"
#define CONNECT_FAILURE "system error: Client connect function failure"
#define WRITE_FAILURE "system error: Client write function failure"
#define READ_FAILURE "system error: Server read function failure"
#define SYSTEM_FAILURE "system error: Server system function failure"



/**
 * The Server function to read data that Clients write and sends by packets
 */

int read_data (int SocketFd, char*Buf,size_t n)
{
    size_t BytesCounter = 0;
    size_t BytesRead = 0;

    while (BytesCounter < n)
    {
        BytesRead = read(SocketFd,Buf,n - BytesCounter);
        if ( BytesRead > 0)
        {
            BytesCounter += BytesRead;
            Buf += BytesRead;
        }

        if (BytesRead < 1)
        {
            return(-1);
        }
    }

    return (int) BytesCounter;
}

/**
 * This function waits for a connection of a Client to the Server
 */


int GetConnection ( int SockFd)
{
    int ClientFd;

    ClientFd = accept(SockFd,nullptr,nullptr);
    if ( ClientFd < 0)
    {
        return -1;
    }
    return ClientFd;

}

/**
 * The socket initializer function according to Server/Client
 */

int SocketInitializer (char ** Arguments, int mode)
{
    char buffer[BUFFER];
    struct sockaddr_in MyAddr{};
    struct hostent *hp;
    gethostname( buffer,BUFFER);
    hp = gethostbyname( buffer);
    if( hp == nullptr)
    {
        std::cerr << HOST_NAME_FAILURE << std::endl;
        return (-1);
    }

    memset(&(MyAddr),0, sizeof(MyAddr)); // Set all My_addr values to zero value
    memcpy( (char *)&MyAddr.sin_addr,hp->h_addr,hp->h_length);     // Initialize the socket address
    MyAddr.sin_family = hp->h_addrtype;     // Initialize to AF_INET
    MyAddr.sin_port = htons( (u_short)strtol(Arguments[2], nullptr,10));     // Set the port number

    // Creating  the socket and the port
    int SockFd = socket(hp->h_addrtype,SOCK_STREAM,0);
    if (SockFd == -1 )
    {
        std::cerr << SOCKET_FAILURE << std::endl;
        return (-1);
    }

    if (mode == 0)
    {
        // Specific actions for Server
        if(bind(SockFd, (struct sockaddr *)&MyAddr,sizeof(MyAddr)) < 0)
        {
            close(SockFd);
            std::cerr << BIND_FAILURE << std::endl;
            return (-1);
        }
        if (listen(SockFd,Five) < 0 )
        {
            std::cerr << LISTEN_FAILURE << std::endl;

        }
    }
    else
    {
        if (connect(SockFd,(struct sockaddr*)&MyAddr, sizeof(MyAddr)) < 0)
        {
            std::cerr << CONNECT_FAILURE << std::endl;
            close(SockFd);

            return (-1);
        }

    }
    return SockFd;

}


/**
 * This function creates a socket for the Client and writes to the Server
 */


void ClientManager(char ** argv)
{
  int SockFd = SocketInitializer(argv,1);
  if (SockFd == -1)
  {
      exit(1);
  }
  if (write(SockFd,argv[3],BUFFER) == -1)
  {
      std::cerr << WRITE_FAILURE << std::endl;
      close(SockFd);
      exit(1);
  }
}


/**
 * This function creates a socket for the Server and wait for a connection of clients.
 * when a connection occurs the Server read data from the Client and execute the command
 */


void ServerManager (char ** argv)
{
    char* buffer[BUFFER];
    int SockFd = SocketInitializer(argv,0);
    if (SockFd == -1)
    {
        exit(1);
    }

    while (true)
    {
        int ClientFd = GetConnection(SockFd);
        if (ClientFd == - 1)
        {
            continue;
        }

        if (read_data(ClientFd,*buffer,BUFFER)  == - 1 )
        {
            close(SockFd);
            std::cerr << READ_FAILURE << std::endl;
            exit(1);
        }
        system(*buffer) ;

    }


}


int main(int argc, char* argv[])
{


    if (strcmp(argv[1],SERVER) == 0 )
    {
        ServerManager(argv);

    } else
    {
        ClientManager(argv);
    }
    return 0 ;

}


