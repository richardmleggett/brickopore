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
#include "ev3_tacho.h"


#define Sleep( msec ) usleep(( msec ) * 1000 )
#define COLOR_COUNT  (( int )( sizeof( color ) / sizeof( color[ 0 ])))
#define COLOR_WHITE 6

const char const *color[] = { "?", "BLACK", "BLUE", "GREEN", "YELLOW", "RED", "WHITE", "BROWN" };
uint8_t sn_color;
uint8_t sn_tacho;
int max_speed;

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
        printf("sn_color = %d\n", sn_color);
    } else {
        printf("COLOR sensor is NOT found\n" );
        uninit_brick();
        exit(1);
    }
    
    printf( "Waiting for tacho motor...\n" );
    while ( ev3_tacho_init() < 1 ) Sleep( 1000 );
    
    printf( "Found tacho motors:\n" );
    for ( i = 0; i < DESC_LIMIT; i++ ) {
        if ( ev3_tacho[ i ].type_inx != TACHO_TYPE__NONE_ ) {
            printf( "  type = %s\n", ev3_tacho_type( ev3_tacho[ i ].type_inx ));
            printf( "  port = %s\n", ev3_tacho_port_name( i, s ));
        }
    }
    
    if (ev3_search_tacho( LEGO_EV3_L_MOTOR, &sn_tacho, 0 )) {
        printf( "LEGO_EV3_L_MOTOR is found...\n" );
        get_tacho_max_speed( sn_tacho, &max_speed );
        printf("  max_speed = %d\n", max_speed );
    } else {
        printf( "LEGO_EV3_L_MOTOR is NOT found\n" );
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

void capture_read(int sockfd)
{
    int val;
    char out_buffer[256];
    char in_buffer[256];
    FLAGS_T state = 1;
    int step_size = 1150;
    int n;
    
    bzero(out_buffer, 256);
    bzero(in_buffer, 256);
    
    printf("Starting motor - running to %d\n", step_size);
    set_tacho_position_sp( sn_tacho, step_size );
    set_tacho_command_inx( sn_tacho, TACHO_RUN_TO_REL_POS );
    
    printf("Sensing\n");
    state = 1;
    while(state != 0) {
        if (!get_sensor_value( 0, sn_color, &val ) || ( val < 0 ) || ( val >= COLOR_COUNT )) {
            val = 0;
        }
        
        out_buffer[0] = '!';
        out_buffer[1] = 'D';
        out_buffer[2] = val;
        out_buffer[3] = 0;
        n = write(sockfd, out_buffer, 4);
        if (n < 0) {
            error("ERROR writing to socket");
        }
        
        //printf("Sent %d\n",val);
        //fflush(stdout);
        
        //n = read(sockfd, in_buffer, 4);
        //if (n < 0) {
        //    error("ERROR reading from socket");
        //}
        //printf("Received %d\n", in_buffer[0]);
        
        Sleep(5);
        
        get_tacho_state_flags( sn_tacho, &state );
    }
    
    printf("Sending stop\n");
    out_buffer[0] = '!';
    out_buffer[1] = 'S';
    out_buffer[2] = 'T';
    out_buffer[3] = 0;
    n = write(sockfd, out_buffer, 4);
    if (n < 0) {
        error("ERROR writing to socket");
    }
    //fflush(stdout);
    Sleep(100);
    
    printf("Reversing\n");
    
    set_tacho_position_sp(sn_tacho, 0-step_size);
    set_tacho_command_inx(sn_tacho, TACHO_RUN_TO_REL_POS);
}

void find_white(void)
{
    int found_white = 0;
    int val;
    int count = 0;
    
    do {
        if (!get_sensor_value( 0, sn_color, &val ) || ( val < 0 ) || ( val >= COLOR_COUNT )) {
            val = 0;
        }

        if (val == COLOR_WHITE) {
            found_white = 1;
        } else {
            printf(" - %d\n", val);
            set_tacho_position_sp(sn_tacho, 10);
            set_tacho_command_inx(sn_tacho, TACHO_RUN_TO_REL_POS);
            count++;
        }
    } while ((!found_white) && (count < 36));
    
    if (found_white == 1) {
        printf("Found sequencing adapter\n");
    } else {
        printf("Did not find sequencing adapter\n");
    }
}

void nudge(int n)
{
    printf("Nudging by %d\n", n);
    set_tacho_position_sp(sn_tacho, n);
    set_tacho_command_inx(sn_tacho, TACHO_RUN_TO_REL_POS);
}

int main(int argc, char *argv[])
{
    int sockfd, n;
    int got_command = 0;
    int running = 1;
    char in_buffer[256];

    
    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }
    
    init_brick();

    printf("Setting up motor\n");
    set_tacho_stop_action_inx( sn_tacho, TACHO_BRAKE );
    set_tacho_speed_sp( sn_tacho, max_speed / 4 );
    set_tacho_ramp_up_sp( sn_tacho, 0 );
    set_tacho_ramp_down_sp( sn_tacho, 0 );
    
    sockfd = open_server_connection(argv[1], atoi(argv[2]));
    bzero(in_buffer, 256);
    
    // Wait for command to sequence
    while (running == 1) {
        n = read(sockfd, in_buffer, 4);
        if (n > 0) {
            if (in_buffer[0] == '!') {
                printf("Got command...\n");
                if (n < 3) {
                    printf("But not long enough\n");
                } else {
                    printf("Command is %c%c\n", in_buffer[1], in_buffer[2]);
                    if ((in_buffer[1] == 'S') && (in_buffer[2] == 'Q')) {
                        capture_read(sockfd);
                    } else if ((in_buffer[1] == 'F') && (in_buffer[2] == 'W')) {
                        find_white();
                    } else if ((in_buffer[1] == 'N') && (in_buffer[2] == 'B')) {
                        nudge(0 - (in_buffer[3]*10));
                    } else if ((in_buffer[1] == 'N') && (in_buffer[2] == 'F')) {
                        nudge(in_buffer[3]*10);
                    } else if ((in_buffer[1] == 'E') && (in_buffer[2] == 'X')) {
                        printf("Got exit command\n");
                        running = 0;
                    }
                }
            }
        }
    }
    
    
    uninit_brick();
    close(sockfd);
    
    return 0;
}
