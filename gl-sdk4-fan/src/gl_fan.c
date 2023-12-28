#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <gl-utils/ubus.h>
#include <jansson.h>
//#include <gl_log.h>
//#include <gl-utils/files.h>

static const char *CPU_TEMP_FILE = "/sys/devices/virtual/thermal/thermal_zone0/temp";
#define FAN_PWM_FILE "/sys/class/thermal/cooling_device0/cur_state"
#define FAN_SPEED_FILE "/sys/class/fan/fan_speed"
#define TEMPERATURE 75
#define PROPORTION 10
#define INTEFRATION 2
#define DIIFFERENTIAL 10
static int temp_div = 1;

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [option]\n"
            "          -T sysfs         # temperature sysfs path, default is %s\n"
            "          -D div         # temperature divide, default is %d\n"
            "          -t temperature   # expected CPU temperature, default is %d\n"
            "          -p proportion    # Proportion parameter in PID algorithm, default is %d\n"
            "          -i integration   # integration parameter in PID algorithm, default is %d\n"
            "          -d differential  # differential parameter in PID algorithm, default is %d\n"
            "          -s               # print fan speed\n"
            "          -v               # verbose\n", prog, CPU_TEMP_FILE, temp_div, TEMPERATURE, PROPORTION, INTEFRATION, DIIFFERENTIAL);
    exit(1);
}

static bool check_file_is_exist(const char *name)
{
    struct stat buffer;
    return (stat(name, &buffer) == 0);
}


static int read_file_oneline(const char *path, char *result, size_t size)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(path, "r");
    if (fp == NULL)
        return -1;

    if ((read = getline(&line, &len, fp)) != -1) {
        if (size != 0)
            memcpy(result, line, size);
        else
            memcpy(result, line, read - 1);
    }

    fclose(fp);
    if (line)
        free(line);
    return 0;
}


static size_t write_file(const char *path, char *buf, size_t len)
{
    FILE *fp = NULL;
    size_t size = 0;
    fp = fopen(path, "w+");
    if (fp == NULL) {
        return 0;
    }
    size = fwrite(buf, len, 1, fp);
    fclose(fp);
    return size;
}

static bool modem_temperature_info(char *recv_buf)
{
    if (0 == access("/proc/gl-hw-info/build-in-modem", F_OK)) {
        char *ptr = NULL;
        json_t *parameter = json_object();
        json_object_set_new(parameter, "cmd", json_string("AT+QTEMP"));
        gl_ubus_par_call("AT", "get_result", json_dumps(parameter, 0), 6 * 1000, &ptr);

        if (NULL != ptr) {
            json_error_t error;
            json_t *tmp = json_loads(ptr, 0, &error);
            const char *result = json_string_value(json_object_get(tmp, "data"));
            strcpy(recv_buf, result);
            return true;
        } else {
            return false;
        }
    }
}

static int get_cell_param(char *buf, char sli, int number, char *result)
{
    int i = 0;
    int cunt = -1;
    char *start = buf;
    int len;
    if (buf == NULL) {
        return -1;
    }
    if (start[0] == '"') { //remove '"'
        start++;
    }
    while ((buf[i] != '\0') && (buf[i] != '\r') && (buf[i] != '\n')) {
        if (buf[i] == sli) {
            if (++cunt == number) {
                len = buf + i - start;
                if (buf[i - 1] == '"') { //remove '"'
                    len--;
                }
                memcpy(result, start, len);
                result[len] = '\0';
                return 0;
            }
            start = buf + i + 1;

            if (start[0] == '"') { //remove '"'
                start++;
            }
        }
        i++;
    }

    char tmp[24];
    strcpy(tmp, buf + i);
    if ((buf[i] == '\r') && (++cunt == number) && (start != buf)) { //get the end param
        len = buf + i - start;
        if (buf[i - 1] == '"') { //remove '"'
            len--;
        }
        memcpy(result, start, len);
        result[len] = '\0';

        return 0;
    }
    return -1;
}

int is_ok(char *buf)
{
    int i = 0;
    for (i = 0; buf[i] != '\0'; i++) {
        if (buf[i] == 'O')
            if (buf[i + 1] == 'K')
                if ((buf[i + 2] == '\r') || (buf[i + 2] == '\n'))
                    return 1;
    }
    return 0;
}

int get_cpu_temp(int *temp)
{
    char tmp[8] = {0};
    char modem_info[128] = {0};
    int modem_cpu_temp = 0;
    if (modem_temperature_info(modem_info)){
        if (is_ok(modem_info)) {
            char buf[128] = {0};
            char modem_temperature[8] = {0};
            char *recv = modem_info;
            for (recv = strstr(recv, "+QTEMP:"); recv != NULL ; recv = strstr(recv, "+QTEMP:")) {
                memset(buf, 0, sizeof(buf));
                sscanf(recv, "%1000[^]%*[^\r\n]", buf);
                strcat(buf, "\r");
                if(strstr(buf, "soc-thermal")){
                    if(0 == get_cell_param(buf + 8, ',', 1, modem_temperature)){
                        modem_cpu_temp = atoi(modem_temperature);
                    }
                    break;
                }
            }
        }
    }

    if (check_file_is_exist(CPU_TEMP_FILE)) {
        read_file_oneline(CPU_TEMP_FILE, tmp, 0);
        int cpu_temp = atoi(tmp) / temp_div;
        cpu_temp = 40;
        if (modem_cpu_temp > cpu_temp)
            *temp = modem_cpu_temp;
        else
            *temp = cpu_temp;

        if (*temp <= 0 || *temp >= 150) {
            //gl_log_err("%d :It's not a normal temperature\n", *temp);
            return -2;
        }
    } else {
        //gl_log_err("%s :No such file or directory\n", CPU_TEMP_FILE);
        return -1;
    }
    return 0;
}

int set_fan_pwm(char pwm)
{
    char tmp[8] = {0};
    int ret = 0;

    sprintf(tmp, "%d\n", pwm);
    if (check_file_is_exist(FAN_PWM_FILE)) {
        if ((ret = write_file(FAN_PWM_FILE, tmp, strlen(tmp))) == 1) {
            return 0;
        } else {
            //gl_log_err("%s :write error: Input/output error\n", FAN_PWM_FILE);
        }
    } else {
        //gl_log_err("%s :No such file or directory\n", FAN_PWM_FILE);
    }
    return -2;
}

int get_fan_speed(void)
{
    char tmp[64] = {0};
    int ret = 0;
    if (check_file_is_exist(FAN_SPEED_FILE)) {
        if ((ret = write_file(FAN_SPEED_FILE, "refresh", strlen("refresh"))) == 1) {
            sleep(2);
            read_file_oneline(FAN_SPEED_FILE, tmp, 0);
            tmp[63] = '\0';
            printf("%s\n", tmp);
            return 0;
        } else {
            //gl_log_err("%s :write error: Input/output error\n", FAN_SPEED_FILE);
        }
    } else {
        //gl_log_err("%s :No such file or directory\n", FAN_SPEED_FILE);
    }
    return -2;
}

int main(int argc, char **argv)
{
    int verbose = 0;
    float prop = PROPORTION, integ = INTEFRATION, diffr = DIIFFERENTIAL;
    int goal_temp = TEMPERATURE;
    int opt;

    //gl_log_level(LOG_ERR);

    while ((opt = getopt(argc, argv, "T:D:t:p:i:d:vs")) != -1) {
        switch (opt) {
            case 'T':
                CPU_TEMP_FILE = optarg;
                break;
            case 'D':
                temp_div = atoi(optarg);
                if (temp_div < 1)
                    temp_div = 1;
                break;
            case 't':
                goal_temp = atoi(optarg);
                break;
            case 'p':
                prop = atof(optarg);
                break;
            case 'i':
                integ = atof(optarg);
                break;
            case 'd':
                diffr = atof(optarg);
                break;
            case 's':
                get_fan_speed();
                return 0;
                break;
            case 'v':
                //gl_log_level(LOG_DEBUG);
                break;
            default: /* '?' */
                usage(argv[0]);
        }
    }

    int current_temp = 0;
    int current_error = 0;
    static int total_error = 0;
    int last_error = 0;
    int prev_error = 0;
    float set_pwm = 0;
    static float last_pwm = 0;

    set_fan_pwm(0);
    while (1) {

        if (get_cpu_temp(&current_temp))
            goto loop_end;

        current_error = current_temp - goal_temp;
        total_error += current_error;
        if (total_error > 120) {
            total_error = 120;
        } else if (total_error < 0 || current_error < -4) {
            total_error = 0;
        }

        set_pwm = prop * current_error - diffr * (current_error - 2 * last_error + prev_error) + integ * total_error;

        prev_error = last_error;
        last_error = current_error;
/*
        gl_log_debug("set_pwm:%f proportion:%f integration:%f differential:%f\n",
                     set_pwm, prop * current_error, integ * total_error, diffr * (current_error - 2 * last_error + prev_error));

        gl_log_debug("current_temp:%d current_error:%d total_error:%d last_error:%d prev_error:%d\n",
                     current_temp, current_error, total_error, last_error, prev_error);
*/

        if (set_pwm > 120) {
            set_pwm = 120;
        } else if (set_pwm < 0) {
            set_pwm = 0;
        }


        if (set_pwm == 0 && last_pwm == 0) {
            goto loop_end;
        } else {
            if (set_pwm == 0 && last_pwm != 0)
                sleep(300);

            last_pwm = set_pwm;
        }

        set_fan_pwm((char)set_pwm);

loop_end:
        sleep(20);
    }

    return 0;
}
