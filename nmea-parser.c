#include <stdio.h>
#include <string.h>

#define MAX_LINE_LENGTH 82
#define GPRMC_CMD "$GPRMC"
#define GPRMC_CMD_LEN 6
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

static void parse_lat(char *deg, struct gps_info *info)
{
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

static int parse(char *line)
{
	struct gps_info info;
	int parse_field=0;
	char *token, *saveptr;
	char *sentence = line;

	token = strtok_r(sentence, ",\n", &saveptr);
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
				return -1;
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
			break;
		case PARSE_LONGT_DIR:
			printf("token = %s\n", token);
			break;
		}
	}

	/* Write to file */
	return 0;
}

static int verify_checksum(char *line)
{
	unsigned checksum_calc=0, org_checksum=1;
	int i;

	for(i=1; (line[i] && line[i]!='*' && i < MAX_LINE_LENGTH); i++)
		checksum_calc ^= line[i];

	if (line[i] == '*')
		org_checksum = strtol(&line[i+1],NULL,16);

//	printf("Calc checksum = %d and org_checksum=%d\n", checksum_calc, org_checksum);
	return (checksum_calc == org_checksum);
}

			
int main()

{
	char line[MAX_LINE_LENGTH];
	char *token;
	char *saveptr;
	char *curr_line = line;
	int parse_line = 0;
	unsigned done_count;

	while (fgets(line, MAX_LINE_LENGTH, stdin)) {
		if (!strncmp(line, GPRMC_CMD, GPRMC_CMD_LEN)) {
			if (!verify_checksum(line)) {
				//printf("checksum failed \n");				
				continue;
			} else {
				//printf("checksum passed\n");
			}

			/* Assuming all data is valid from this point on */
			parse(line);
			if (++done_count == 100) {
		//		close_file();
				return 0;
			}
		}	
	}

	return 0;
}
