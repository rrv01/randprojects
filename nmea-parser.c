#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#define MAX_LINE_LENGTH 82
#define GPRMC_CMD "GPRMC"
#define GPRMC_CMD_LEN 5
#define GPS_ACTIVE 'A'
#define PARSE_TIME 2
#define PARSE_STATUS 3
#define PARSE_LAT 4
#define PARSE_LAT_DIR 5
#define PARSE_LONGT 6
#define PARSE_LONGT_DIR 7

/*
RMC - NMEA has its own version of essential gps pvt (position, velocity, time) data. It is called RMC, The Recommended Minimum, which will look similar to:

$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A

Where:
     RMC          Recommended Minimum sentence C
     123519       Fix taken at 12:35:19 UTC
     A            Status A=active or V=Void.
     4807.038,N   Latitude 48 deg 07.038' N
     01131.000,E  Longitude 11 deg 31.000' E
     022.4        Speed over the ground in knots
     084.4        Track angle in degrees True
     230394       Date - 23rd of March 1994
     003.1,W      Magnetic Variation
     *6A          The checksum data, always begins with *
*/

struct gps_info {
	struct time {
		unsigned hours;
		unsigned mins;
		unsigned secs;
	}time;
	int lat;
	unsigned lat_factor;
	int longt;
	unsigned longt_factor;
};

//printf("token = %s\n", token);
/* for (token=curr_line; token != NULL; curr_line = NULL) {
			token = strtok_r(curr_line, ",\n", &saveptr);

*/

static void parse_time(char *time, struct gps_info *info)
{
	info->time.hours = (time[0] - '0')*10 + (time[1] - '0'); 
	info->time.mins = (time[2] - '0')*10 + (time[3] - '0'); 
	info->time.secs = (time[4] - '0')*10 + (time[5] - '0'); 
}

static inline int parse_status(char *status)
{
	return (status[0] == 'A');
}

static void parse_lat(char *data, struct gps_info *info)
{
	int i,j;
	int len = strlen(data);
	double mins = 0;
	unsigned mul = 1, storemul;
	unsigned degs = 0;

	for(i=len-1;;i--)
		if (data[i] == '.')
			break;

	i -=2;

	mins = strtod(&data[i], NULL);
	printf("mins in str = %s\n", (char*)&data[i]);
	printf("mins %lf \n", mins);

	data[i] = '\0';
	printf("degs in str = %s\n", data);
	degs = strtol(data, NULL, 10);
	printf("Degs %d \n", degs);

/*	
	for(i=len-1;; i--) {
		if (data[i] == '.') {
			mins += (data[--i] - '0')*mul;
			mul *=10;
			mins += (data[--i] - '0')*mul;
			break;
		}
		mins += (data[i] - '0')*mul;
		mul *= 10;
	}

	mins = mins/mul;
	mins = mins/60;
	storemul = mul;

	mul = 1;
	for(j=i-1; j>=0; j--) {
		degs += (data[j] - '0')*mul;
		mul = mul*10;
	}

	degs = degs + mins;
	degs = degs*storemul;
	printf("STORE DEgs = %d and factor=%d\n", (unsigned)degs , storemul);
*/

}

static inline void parse_lat_direction(char *dir, struct gps_info *info)
{
	if (dir[0] == 'S')
		info->lat *= -1;
}

static inline void parse_longt_direction(char *dir, struct gps_info *info)
{
	if (dir[0] == 'W')
		info->longt *= -1;
}

struct token_data {
	char line[MAX_LINE_LENGTH];
	int calc_checksum;
	int order;
	char org_checksum[3];
};

static inline int verify_checksum(struct token_data *data)
{
	long org_checksum = strtol(data->org_checksum, NULL, 16);

	printf("Calc checksum = %d and org_checksum=%ld\n", data->calc_checksum, org_checksum);
	return (data->calc_checksum != org_checksum);
}

static void *parse_line(void *data)
{
	struct gps_info info;
	int parse_field = 0;
	char *token, *saveptr;
	struct token_data *tdata = data;
	char *sentence = tdata->line;

	if (verify_checksum(tdata))
		goto out;

	token = strtok_r(sentence, ",\n", &saveptr);

	if (strncmp(token,GPRMC_CMD, GPRMC_CMD_LEN))
		goto out;

	printf("CMD parsing done\n");
	parse_field++;
	while(token != NULL) {
		token = strtok_r(NULL, ",\n", &saveptr);
		parse_field++;

		switch (parse_field) {
		case PARSE_TIME:
			parse_time(token, &info);
			printf("token = %s\n", token);
			break;
		case PARSE_STATUS:
			printf("token = %s\n", token);
			if(!parse_status(token))
				goto out;
			break;
		case PARSE_LAT:
			printf("token = %s\n", token);
			parse_lat(token, &info);
			break;
		case PARSE_LAT_DIR:
			printf("token = %s\n", token);
			parse_lat_direction(token, &info);
			break;
		case PARSE_LONGT:
			printf("token = %s\n", token);
			parse_lat(token, &info);
			break;
		case PARSE_LONGT_DIR:
			printf("token = %s\n", token);
			break;
		}
	}

	/* Write to file */
out:
	pthread_exit(NULL);
	return NULL;
}

int main()

{
	char line[MAX_LINE_LENGTH] = {0};
	char c;
	long calc_checksum = 0;
	int done = 0;
	int count = 0;
	int i;
	char str_checksum[3] = {0};
	pthread_t th;
        static struct termios oldt, newt;

        /*tcgetattr gets the parameters of the current terminal
        STDIN_FILENO will tell tcgetattr that it should write the settings
        of stdin to oldt*/
        tcgetattr(STDIN_FILENO, &oldt);
        /*now the settings will be copied*/
        newt = oldt;

        /*ICANON normally takes care that one line at a time will be processed
        that means it will return if it sees a "\n" or an EOF or an EOL*/
        newt.c_lflag &= ~(ICANON);          

        /*Those new settings will be set to STDIN
        TCSANOW tells tcsetattr to change attributes immediately. */
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	while ((c = getchar()) != '\n' && c!='\r') {
//		printf("got char %c\n",c);
		if (c == '$') {
			i = 0;
			calc_checksum = 0;
			done = 0;
			continue;
		}
		line[i++] = c;
		if (c == '*') {
			str_checksum[0] = getchar();
			str_checksum[1] = getchar();
			done = 1;
		} else {
			calc_checksum ^= c;
		}
		if (done) {
			struct token_data *token = malloc(sizeof(struct token_data));
			strncpy(token->line, line, MAX_LINE_LENGTH);
			strncpy(token->org_checksum, str_checksum, 3);
			token->calc_checksum = calc_checksum;
			token->order = count;
			pthread_create(&th, NULL, parse_line, token);
			count++;
			done = 0;
			printf("thread created \n");
		}	
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	pthread_exit(NULL);
	return 0;
}
