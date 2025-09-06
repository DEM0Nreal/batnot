#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

static char *pid_file_path;
static char *log_file_path;
static FILE *pid_file;
static FILE *log_file;
static size_t   discharging_warning_points_size;
static int  *discharging_warning_points;

// handle SIGTERM signal
void signal_handler_SIGTERM(int sig){
    // get current time and log
    time_t now = time(NULL);
    char *t = ctime(&now);
    t[strlen(t) - 1] = '\0';
    printf("%s: SIGTERM, terminating\n", t);

    // remove pid file to allow starting this daemon again
    if (remove(pid_file_path) != 0){
        fprintf(stderr, "FAILED TO REMOVE pid_file_path (%s)\n", pid_file_path);
        fflush(stderr);
    }

    fprintf(stdout, "\r\n");
    fflush(stdout);

    free(pid_file_path);
    free(log_file_path);

    exit(EXIT_SUCCESS);
}

// simple notification sending function
// urgency: 0 - low    1 - normal    2 - critical
// msg:     message displayed in the notification
void notification_send(int urgency, char *msg){
    // 512 might be an overkill but it works.
    char *command = malloc(512);
    strcpy(command, "notify-send "); 

    switch (urgency){
        case 0:
            strcat(command, "--urgency=\"low\" ");
            break;
        case 1:
            strcat(command, "--urgency=\"normal\" ");
            break;
        case 2:
            strcat(command, "--urgency=\"critical\" ");
            break;
        default:
            // if urgency is not 0, 1 or 2 something went horribly wrong
            fprintf(stderr, "unsupported urgency!! Raising SIGTERM...\n");
            free(command);
            if (raise(SIGTERM)){
                fprintf(stderr, "FATAL ERROR! failed to raise SIGTERM\n");
                exit(EXIT_FAILURE);
            }
    }

    strcat(command, "\""); // body prefix
    strcat(command, msg);
    strcat(command, "\""); // body postfix

    // execute command
    system(command);

    free(command);
}

// returns battery level in percentage
int getBatLevel(){ 
    int capacity = -1;

    char *capacity_file_path = "/sys/class/power_supply/BAT0/capacity";
    FILE *capacity_fd = fopen(capacity_file_path, "r");
    assert(capacity_fd != NULL);

    assert(fscanf(capacity_fd, "%d", &capacity) == 1);
    
    fclose(capacity_fd);
    return capacity;
}

// returns whether battery is charging
// 0 -> discharging (not charging)
// 1 -> charging
// 2 -> battery is full
int getBatCharging(){
    int charging = 0;
    char *charging_file_path = "/sys/class/power_supply/BAT0/status";
    FILE *charging_fd = fopen(charging_file_path, "r");
    assert(charging_fd != NULL);

    char firstChar = fgetc(charging_fd); // D -> Discharging, C -> Charging, F -> Full
    assert(firstChar != EOF);

    if (firstChar == 'D'){
        charging = 0;
    }
    if (firstChar == 'C'){
        charging = 1;
    }
    if (firstChar == 'F'){
        charging = 2;
    }

    fclose(charging_fd);
    return charging;
}

int main(void){
    // pid and log file names
    char *pid_file_name = "/.batnot.pid";
    char *log_file_name = "/.batnot.log";

    // file containing points below which a notification should be sent
    char *discharging_file_name = "/.batnot_discharging_warning";


    // directory in which pid and log files are expected to be, here $HOME
    char *home_directory = malloc(strlen(getenv("HOME")));
    assert(home_directory != NULL);
    strcpy(home_directory, getenv("HOME"));

    pid_file_path = malloc(strlen(home_directory) + strlen(pid_file_name) + 1);
    assert(pid_file_path != NULL);
    strcpy(pid_file_path, home_directory);
    strcat(pid_file_path, pid_file_name);

    log_file_path = malloc(strlen(home_directory) + strlen(log_file_name) + 1);
    assert(log_file_path != NULL);
    strcpy(log_file_path, home_directory);
    strcat(log_file_path, log_file_name); 

    // get array of percentage warnings when discharging
    char *discharging_file_path = malloc(strlen(home_directory) + strlen(discharging_file_name) + 1);
    assert(discharging_file_path != NULL);
    strcpy(discharging_file_path, home_directory);
    strcat(discharging_file_path, discharging_file_name);
    FILE *discharging_fd = fopen(discharging_file_path, "r");
    assert(discharging_fd != NULL);

    char *discharging_values_string = malloc(512);
    assert(discharging_values_string != NULL);

    assert(fgets(discharging_values_string, 512, discharging_fd) != NULL);
    char *token;
    token = strtok(discharging_values_string, " ");
    discharging_warning_points_size = 0;
    while (token != NULL) {
        discharging_warning_points = realloc(discharging_warning_points, 
                                       (discharging_warning_points_size + 1) * sizeof(int));
        assert(discharging_warning_points != NULL);

        discharging_warning_points[discharging_warning_points_size] = atoi(token);
        discharging_warning_points_size++;

        token = strtok(NULL, " ");
    }

    // debug statement
    //for (int i = 0; i < discharging_warning_points_size; ++i){
    //    printf("warning at: %d\n", discharging_warning_points[i]);
    //}
    
    fclose(discharging_fd);
    free(discharging_file_path);
    free(discharging_values_string);
    free(token);


    // double fork in order to work as daemon
    pid_t child_pid = fork();
    assert(child_pid != -1);
    if (child_pid > 0)
        exit(EXIT_SUCCESS);

    setsid();
    child_pid = fork();
    assert(child_pid != -1);
    if (child_pid > 0)
        exit(EXIT_SUCCESS);

    // get pid, if pid_file_path already exists, assert is called
    pid_t pid = getpid();
    int pid_fd = open(pid_file_path, O_EXCL | O_CREAT | O_WRONLY | O_TRUNC, 0644);
    assert(pid_fd != -1);
    
    pid_file = fdopen(pid_fd, "w");
    assert(pid_file != NULL);
    fprintf(pid_file, "%d", pid);
    fclose(pid_file);

    log_file = fopen(log_file_path, "a");
    assert(log_file != NULL);

    // set log_file_path as stdout and stderr
    setlinebuf(log_file);
    setlinebuf(stdout);
    setlinebuf(stderr);
    int log_fd = fileno(log_file);
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);

    free(home_directory);

    // handle signals
    signal(SIGTERM, signal_handler_SIGTERM);

    // find smallest discharging point to use it for critical urgency notification
    int smallest_discharging_point = 100;
    for (int i = 0; i < discharging_warning_points_size; ++i){
        int candidate = discharging_warning_points[i];
        if (candidate < smallest_discharging_point){
            smallest_discharging_point = candidate;
        }
    }

    int prevBatLevel = -1;
    int prevBatStatus = -1;
    while (true){
        // get time for log
        time_t now = time(NULL);
        char *t = ctime(&now);
        t[strlen(t) - 1] = '\0';

        int batLevel = getBatLevel();
        int charging  = getBatCharging();

        // battery status change
        if (prevBatStatus != charging){
            prevBatStatus = charging;
            if (charging == 0){
                notification_send(1, "battery is Discharging");
                printf("%s: battery changed state to Discharging\n", t);
            }
            if (charging == 1){
                notification_send(1, "battery is Charging");
                printf("%s: battery changed state to Charging\n", t);
            }
            if (charging == 2){
                notification_send(1, "battery is Full");
                printf("%s: battery changed state to Full\n", t);
            }
        }

        // getting to and below warning level
        if (prevBatLevel != batLevel && charging == 0){
            if (discharging_warning_points_size <= 0) {
                fprintf(stderr, "%s: no discharging warning points found!\n", t);
                break;
            }

            // find which point of discharging has been reached
            int smallest = 101; // 101 should be bigger than possible value here
            for (int i = 0; i < discharging_warning_points_size; ++i){
                int candidate = discharging_warning_points[i];
                if (candidate >= batLevel && candidate < smallest){
                    smallest = candidate;
                }
            }

            // makes sure it doesn't spam notifications
            if (smallest <= 100 && smallest >= batLevel && smallest != prevBatLevel){
                prevBatLevel = batLevel;
                int urgency = 1;
                if (smallest == smallest_discharging_point) urgency = 2;
                char *notification_body_buffer = malloc(256);
                sprintf(notification_body_buffer, "Battery's at %d%%! Charge it!", batLevel);
                
                notification_send(urgency, notification_body_buffer);
                free(notification_body_buffer);
                printf("%s: warning sent at %d%%\n", t, batLevel);
            }
        }


        sleep(30); // update every this many seconds
    }

    if (remove(pid_file_path) != 0){
        fprintf(stderr, "FAILED TO REMOVE pid_file_path (%s)\n", pid_file_path);
        fflush(stderr);
    }
    free(pid_file_path);
    free(log_file_path);

    return 0;
}
