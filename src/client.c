#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int open_server_connection(char* server, int portno)
{
    struct sockaddr_in serv_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockfd < 0) {
        error("ERROR opening socket");
    }
    
    server = gethostbyname(server);
    
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
        error("ERROR connecting");
    }
    
    return sockfd;
}

int main(int argc, char *argv[])
{
    int sockfd, n;
    struct hostent *server;
    char buffer[256];
    
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    
    sockfd = open_server_connection(argv[1], atoi(argv[2]));
    
    printf("Please enter the message: ");

    bzero(buffer,256);
    fgets(buffer,255,stdin);
    
    n = write(sockfd,buffer,strlen(buffer));
    
    if (n < 0) {
        error("ERROR writing to socket");
    }
    
    bzero(buffer,256);
    
    n = read(sockfd,buffer,255);
    
    if (n < 0) {
        error("ERROR reading from socket");
    }
    
    printf("%s\n",buffer);
    
    return 0;
}
