#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

#define MAX_OUTPUT_GPS_LINES 100
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

#ifdef DEBUG
#define pr_debug printf
#else
#define pr_debug
#endif

#ifdef PRINFO
#define pr_info printf
#else
#define pr_info
#endif

FILE *fp;
int order_num, lines_written;
pthread_mutex_t cond_var_lock = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;

struct gps_info {
	unsigned hours;
	unsigned mins;
	unsigned secs;
	int lat;
	unsigned lat_factor;
	int longt;
	unsigned longt_factor;
};

struct token_data {
	char line[MAX_LINE_LENGTH];
	int calc_checksum;
	int order;
	char org_checksum[3];
};

static inline void parse_time(char *time, struct gps_info *info)
{
	info->hours = (time[0] - '0')*10 + (time[1] - '0'); 
	info->mins = (time[2] - '0')*10 + (time[3] - '0'); 
	info->secs = (time[4] - '0')*10 + (time[5] - '0'); 
	pr_debug("Time calc as %d:%d:%d\n", info->hours, info->mins, info->secs);
}

static inline int parse_status(char *status)
{
	return (status[0] == 'A');
}

static void parse_degrees(char *data, struct gps_info *info, int param)
{
	int i,j;
	int len = strlen(data);
	double mins = 0, val;
	int degs = 0, final;
	int mul = 0;
	char str_mins[10] = {0};
	int pow10[10] = {1, 10, 100, 1000, 10000, 100000, 1000000,
			10000000, 100000000, 1000000000};

	val = strtod(data, NULL);
	degs = val / 100;
	pr_debug("Degs %d \n", degs);

	mins = (val - degs*100) / 60;
	pr_debug("Mins %lf \n", mins);

	sprintf(str_mins,"%lf",mins);
	mul = strlen(str_mins) - 2;

	val = (degs+mins) * pow10[mul];
	final = (int)val;

	pr_debug("val = %lf final val %d mul %d \n", val, final, mul);

	switch (param) {
		case PARSE_LAT:
			info->lat = final;
			info->lat_factor = pow10[mul];
			break;
		case PARSE_LONGT:
			info->longt = final;
			info->longt_factor = pow10[mul];
			break;
	}
}

static inline void parse_direction(char *dir, struct gps_info *info, int param)
{
	switch(param) {
	case PARSE_LAT_DIR:
		if (dir[0] == 'S')
			info->lat *= -1;
		break;
	case PARSE_LONGT_DIR:
		if (dir[0] == 'W') {
			info->longt *= -1;
		}
		break;
	}
}

static inline int verify_checksum(struct token_data *data)
{
	long org_checksum = strtol(data->org_checksum, NULL, 16);

	pr_debug("Calc checksum = %d and org_checksum=%ld\n", data->calc_checksum, org_checksum);
	return (data->calc_checksum == org_checksum);
}

static void *parse_line(void *data)
{
	struct gps_info info;
	int parse_field = 0;
	char *token, *saveptr;
	struct token_data *tdata = data;
	char *sentence = tdata->line;
	char output[35] = {0};
	int valid_data = 0;

	if (!verify_checksum(tdata))
		goto out;

	token = strtok_r(sentence, ",\n", &saveptr);

	if (strncmp(token, GPRMC_CMD, GPRMC_CMD_LEN))
		goto out;

	pr_debug("CMD parsing done\n");
	parse_field++;
	while(token != NULL) {
		token = strtok_r(NULL, ",\n", &saveptr);
		parse_field++;

		switch (parse_field) {
		case PARSE_TIME:
			parse_time(token, &info);
			pr_debug("token = %s\n", token);
			break;
		case PARSE_STATUS:
			pr_debug("token = %s\n", token);
			if(!parse_status(token))
				goto out;
			break;
		case PARSE_LAT:
			pr_debug("token = %s\n", token);
			parse_degrees(token, &info, PARSE_LAT);
			break;
		case PARSE_LAT_DIR:
			pr_debug("token = %s\n", token);
			parse_direction(token, &info, PARSE_LAT_DIR);
			break;
		case PARSE_LONGT:
			pr_debug("token = %s\n", token);
			parse_degrees(token, &info, PARSE_LONGT);
			break;
		case PARSE_LONGT_DIR:
			pr_debug("token = %s\n", token);
			parse_direction(token, &info, PARSE_LONGT_DIR);
			valid_data = 1;
			break;
		}
	}

	/* Write to file */
out:
	pthread_mutex_lock(&cond_var_lock);
	while (order_num != tdata->order)
		pthread_cond_wait(&cond_var, &cond_var_lock);

	if (valid_data) {
		fprintf(fp,"%d:%d:%d,%lf,%lf\n",info.hours,info.mins,info.secs,
			info.lat*1.0/info.lat_factor, info.longt*1.0/info.longt_factor);
		fsync(fileno(fp));
		pr_info("\nline_written to file %d\n", order_num);
		lines_written++;
	} else {
		pr_info("\nINVALID data skipped order %d\n",tdata->order);
	}
	
	order_num++;
	if (lines_written == MAX_OUTPUT_GPS_LINES) {
		fclose(fp);
		pr_info("DONE FP CLOSED\n");
	}
	pthread_cond_broadcast(&cond_var);
	pthread_mutex_unlock(&cond_var_lock);

	free(tdata->line);
	pthread_exit(NULL);
	return NULL;
}

int main()
{
	char line[MAX_LINE_LENGTH] = {0};
	char c;
	long calc_checksum = 0;
	int done = 0, count = 0, ret = 0;
	int i;
	char str_checksum[3] = {0};
        static struct termios oldt, newt;
	pthread_t th;

	fp = fopen("gps_output_data.txt", "w");
	if (!fp) {
		printf("Error: Cannot open gps_output_data.txt for writing : %s\n", strerror(errno));
		return -EINVAL;
	}

        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON);          
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	while ((c = getchar()) != '\n' && c!='\r') {
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
			if (!token) {
				printf("Error: Could not allocate memory for token\n");
				ret = -ENOMEM;
				goto out_done;
			}
			strncpy(token->line, line, MAX_LINE_LENGTH);
			strncpy(token->org_checksum, str_checksum, 3);
			token->calc_checksum = calc_checksum;
			token->order = count;
			pthread_create(&th, NULL, parse_line, token);
			count++;
			done = 0;
			pr_debug("Thread created %d\n", count);
		}	
		if (lines_written == MAX_OUTPUT_GPS_LINES)
			goto out_done;
	}

out_done:
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	pthread_exit(NULL);
	return ret;
}
