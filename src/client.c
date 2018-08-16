#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <unistd.h>
#include "ev3.h"
#include "ev3_port.h"
#include "ev3_sensor.h"

#define Sleep( msec ) usleep(( msec ) * 1000 )
#define COLOR_COUNT  (( int )( sizeof( color ) / sizeof( color[ 0 ])))

const char const *color[] = { "?", "BLACK", "BLUE", "GREEN", "YELLOW", "RED", "WHITE", "BROWN" };

void error(char *msg)
{
    perror(msg);
    exit(0);
}

void uninit_brick(void)
{
    ev3_uninit();
}

void init_brick(void)
{
    char s[ 256 ];
    int val;
    uint32_t n, i, j;
    uint8_t sn_touch, sn_color, sn_ir;
    
    printf("Waiting for the EV3 brick...\n");
    if (ev3_init() < 1) {
        exit(1);
    }
    
    printf("Got EV3 brick...\n");

    ev3_sensor_init();

    printf("Found sensors:\n" );
    for (i = 0; i < DESC_LIMIT; i++) {
        if (ev3_sensor[ i ].type_inx != SENSOR_TYPE__NONE_) {
            printf( "  type = %s\n", ev3_sensor_type( ev3_sensor[ i ].type_inx));
            printf( "  port = %s\n", ev3_sensor_port_name( i, s));
            if (get_sensor_mode( i, s, sizeof(s))) {
                printf("  mode = %s\n", s);
            }
            if (get_sensor_num_values( i, &n)) {
                for (j = 0; j < n; j++ ) {
                    if ( get_sensor_value(j, i, &val )) {
                        printf( "  value%d = %d\n", j, val);
                    }
                }
            }
        }
    }
    
    if (ev3_search_sensor(LEGO_EV3_COLOR, &sn_color, 0)) {
        set_sensor_mode( sn_color, "COL-COLOR" );
    } else {
        printf("COLOR sensor is NOT found\n" );
        uninit_brick();
        exit(1);
    }
}

int open_server_connection(char* server_name, int portno)
{
    struct sockaddr_in serv_addr;
    struct hostent *server;   
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    printf("Opening connection to %s port %d\n", server_name, portno);

    if (sockfd < 0) {
        error("ERROR opening socket");
    }
    
    server = gethostbyname(server_name);
    
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

    fflush(stdout);
    
    return sockfd;
}

int main(int argc, char *argv[])
{
    int sockfd, n, val;
    char out_buffer[256];
    char in_buffer[256];
    uint8_t sn_color;

    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    
    init_brick();
    
    sockfd = open_server_connection(argv[1], atoi(argv[2]));
    bzero(out_buffer, 256);
    bzero(in_buffer, 256);

    printf("Sensing...");
    while(1) {
        if (!get_sensor_value( 0, sn_color, &val ) || ( val < 0 ) || ( val >= COLOR_COUNT )) {
            val = 0;
        }

        out_buffer[0] = val;
        out_buffer[1] = 0;
	n = write(sockfd, out_buffer, 2);
	if (n < 0) {
	    error("ERROR writing to socket");
        }

        printf("Sent %d\n",val);
	fflush(stdout);

        n = read(sockfd, in_buffer, 2);
	if (n < 0) {
	    error("ERROR reading from socket");
	}
        printf("Received %d\n", in_buffer[0]);

        Sleep(100);
    }

    uninit_brick();
    
    return 0;
}
