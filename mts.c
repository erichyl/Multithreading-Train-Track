#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h> 
#include <sys/time.h>
#include <ctype.h>

#define TIME_INCREMENT 10000 // 0.1 seconds

// Train struct
typedef struct {
    int id;             // train ID
    char direction;     // direction of which the train is travelling (West or East)
    int priority;       // priority of the station (1 = high, 0 = low)
    int loading_time;   // time to load the train
    int crossing_time;  // time for train to cross the main track
    struct Train* next; // pointer to the next train in the queue
} Train;

// Queue struct
typedef struct {
    Train* front;       // train at the front of the queue
    Train* rear;        // train at the back of the queue
} Queue;

// initialize mutexes
pthread_mutex_t track_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t track_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;

// initialize train waiting queues
Queue high_east_queue = {NULL, NULL};
Queue high_west_queue = {NULL, NULL};
Queue low_east_queue = {NULL, NULL};
Queue low_west_queue = {NULL, NULL};

// saves the size of each queue
int queueSizes[] = {0, 0, 0, 0};  // 0 = HE, 1 = HW, 2 = LE, 3 = LW

// global variables
int global_time = 0;         // global time
char curr_dir = 'b';         // 'b' = both, 'w' = west, 'e' = east
char next_dir = 'w';         // 'b' = both, 'w' = west, 'e' = east
int track_occupied = 0;      // 0 or 1, boolean for if the track is currently occupied
int highestPrioTrainID = 0;  // id number of the highest priority train (next to depart)
int loading1;                // variable to save loading time, used for finding highestPrioTrainID
int loading2;                // variable to save another loading time, used for finding highestPrioTrainID

// intializes all trains from the input file
void initTrains(char *filename, Train *trains, int *num_trains) {
    // max line length of 20
    char line[10];
    // open file
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Unable to open file");
        return;
    }
    
    // tokenize each line
    int i = 0;
    while (fgets(line, sizeof(line), file) && i < 75) {
        char *token = strtok(line, " \t\n");
        trains[i].id = i;
        if (token) {
            trains[i].direction = token[0];
            token = strtok(NULL, " \t\n");
        }
        // determine priority from direction character
        if (trains[i].direction == 'W' || trains[i].direction == 'E') {
            trains[i].priority = 1;
            // standardize direction characters as lowercase letters
            trains[i].direction = tolower(trains[i].direction);
        } else {
            trains[i].priority = 0;
        }
        if (token) {
            trains[i].loading_time = atoi(token);
            token = strtok(NULL, " \t\n");
        }
        if (token) {
            trains[i].crossing_time = atoi(token);
            i++;
        }
    }
    // update number of trains and close file
    *num_trains = i;
    fclose(file);
}

// add a train to the back of the queue
void enqueue(Queue* queue, Train* train) {
    train->next = NULL;
    // if queue is empty, set front and rear to this train
    if (queue->rear == NULL) {
        queue->front = queue->rear = train;
    } else {
        queue->rear->next = train;
        queue->rear = train;
    }
}

// remove the train with id == tid from the queue
Train* dequeue(Queue* queue, int tid) {
    // check if queue is empty
    if (queue->front == NULL) {
        return NULL;
    }

    // intialize trains
    Train* train = queue->front;
    Train* previous = NULL;

    // find the train with id == tid
    while (train != NULL) {
        if (train->id == tid) {
            break;
        }
        previous = train;
        train = train->next;
    }

    // if tid is not found, return NULL
    if (train == NULL) {
        return NULL;  
    }

    // if tid was at the very front
    if (previous == NULL) {
        queue->front = train->next;
    } else {
        previous->next = train->next;
    }

    // update rear if needed
    if (queue->rear == train) {
        queue->rear = previous;
    }
    
    // return the train with id = tid
    return train;
}

// return the direction of train as a string for printing
const char* get_direction_string(char direction) {
    return (direction == 'e') ? "East" : "West";
}

// print the current time since the simulation began
void print_time(int time) {
    int seconds = time / 10;
    int hundredths = time % 10;
    printf("%02d:%02d:%02d.%d", seconds / 3600, (seconds / 60) % 60, seconds, hundredths);
}

// clock thread using time.h
void* clock_thread(void* arg) {
    struct timeval start_time, current_time;
    // get the initial time when the thread starts
    gettimeofday(&start_time, NULL); 
    
    while (true) {
        // sleep for 0.1 seconds
        usleep(TIME_INCREMENT); 
        
        // get the current time in microseconds
        gettimeofday(&current_time, NULL); 
        
        // calculate the difference in time (in microseconds)
        long elapsed_time_us = (current_time.tv_sec - start_time.tv_sec) * 1000000 + (current_time.tv_usec - start_time.tv_usec);
        
        // convert microseconds to hundredths of a second (1 second = 100000 microseconds = 1000 hundredths)
        global_time = (int)(elapsed_time_us / 10000); 
    }
    return NULL;
}

// function to find the highest priority train in the queue
// aka the train that should go on the track next
void findHighestPrioTrain() {
    // lock 
    pthread_mutex_lock(&track_mutex);
    // reset highestPrioTrainID
    highestPrioTrainID = -1;
    // trains at the front of each queue
    Train *front1 = high_east_queue.front;
    Train *front2 = high_west_queue.front;
    // initialize loading times to extremely high
    loading1 = 99999;
    loading2 = 99999;
    // find the frontmost train in the queue that meets the direction requirement
    while (front1 != NULL) {
        if ((front1->direction == next_dir) || next_dir == 'b') {
            loading1 = front1->loading_time;
            break;
        }
        front1 = front1->next;
    }

    // find the frontmost train in the queue that meets the direction requirement
    while (front2 != NULL) {
        if ((front2->direction == next_dir) || next_dir == 'b') {
            loading2 = front2->loading_time;
            break;
        }
        front2 = front2->next;
    }

    // the smallest valid loading time is the highest priority amongst high priorty queues
    if (loading1 < loading2) {
        highestPrioTrainID = front1->id;
    } else if (loading1 > loading2) {
        highestPrioTrainID = front2->id;
    } else if (loading1 == loading2 && loading1 < 99999) {
        if (front1->id < front2->id) {
            highestPrioTrainID = front1->id;
        } else {
            highestPrioTrainID = front2->id;
        }
    } else {
        // if there were no trains in high priority queues, check the low priority queues
        // trains at the front of each queue
        front1 = low_east_queue.front;
        front2 = low_west_queue.front;
        // reintialize loading1 and loading2
        loading1 = 99999;
        loading2 = 99999;

        // find the frontmost train in the queue that meets the direction requirement
        while (front1 != NULL) {
            if ((front1->direction == next_dir) || next_dir == 'b') {
                loading1 = front1->loading_time;
                break;
            }
            front1 = front1->next;
        }

        // find the frontmost train in the queue that meets the direction requirement
        while (front2 != NULL) {
            if ((front2->direction == next_dir) || next_dir == 'b') {
                loading2 = front2->loading_time;
                break;
            }
            front2 = front2->next;
        }

        // the smallest valid loading time is the highest priority amongst low priorty queues
        if (loading1 < loading2) {
            highestPrioTrainID = front1->id;
        } else if (loading1 > loading2) {
            highestPrioTrainID = front2->id;
        } else if (loading1 == loading2 && loading1 < 99999) {
            if (front1->id < front2->id) {
                highestPrioTrainID = front1->id;
            } else {
                highestPrioTrainID = front2->id;
            }
        }
    }
    // if no valid trains meet the criteria, 
    // send the train with the priority and closest to the front of the queue
    if (highestPrioTrainID == -1) {
        if (queueSizes[0] > 0) {
            highestPrioTrainID = high_east_queue.front->id;
        } else if (queueSizes[1] > 0) {
            highestPrioTrainID = high_west_queue.front->id;
        } else if (queueSizes[2] > 0) {
            highestPrioTrainID = low_east_queue.front->id;
        } else if (queueSizes[3] > 0) {
            highestPrioTrainID = low_west_queue.front->id;
        }
    }
    // unlock
    pthread_mutex_unlock(&track_mutex);
}

void* train_thread(void* arg) {
    // initialize current train
    Train* train = (Train*) arg;
    // start time of simulation
    int train_start_time;

    // get the start time
    train_start_time = global_time;  

    // wait for the train to load
    while (true) {
        // lock clock mutex
        pthread_mutex_lock(&clock_mutex);
        // if load time is over, unlock and break
        if (global_time >= train->loading_time + train_start_time) {
            pthread_mutex_unlock(&clock_mutex);
            break;
        }
        // keep waiting
        pthread_mutex_unlock(&clock_mutex);
        usleep(TIME_INCREMENT);
    }

    // print "ready to go" when the loading time finishes
    pthread_mutex_lock(&clock_mutex);
    print_time(global_time);
    printf(" Train %2d is ready to go %s\n", train->id, get_direction_string(train->direction));

    // if its the first train in all of the queues, its the next train to depart
    if (queueSizes[0] + queueSizes[1] + queueSizes[2] + queueSizes[3] == 0) {
        highestPrioTrainID = train->id;
    }

    // add train to the correct queue based on priority and direction
    if (train->priority == 1) {
        if (train->direction == 'e') {
            enqueue(&high_east_queue, train);
            queueSizes[0]++;
        } else {
            enqueue(&high_west_queue, train);
            queueSizes[1]++;
        }
    } else {
        if (train->direction == 'e') {
            enqueue(&low_east_queue, train);
            queueSizes[2]++;
        } else {
            enqueue(&low_west_queue, train);
            queueSizes[3]++;
        }
    }
    // unlock
    pthread_mutex_unlock(&clock_mutex);

    // find the next train to depart
    findHighestPrioTrain();

    // wait until it's this train's turn to go
    while (track_occupied || (highestPrioTrainID != train->id)) {
        pthread_mutex_lock(&track_mutex);
        pthread_cond_wait(&track_cond, &track_mutex);
        pthread_mutex_unlock(&track_mutex);
    }

    // dequeue the train since it's departing
    // based on direction and priority
    if (train->priority == 1) {
        if (train->direction == 'e') {
            dequeue(&high_east_queue, train->id);
            queueSizes[0]--;
        } else {
            dequeue(&high_west_queue, train->id);
            queueSizes[1]--;
        }
    } else {
        if (train->direction == 'e') {
            dequeue(&low_east_queue, train->id);
            queueSizes[2]--;
        } else {
            dequeue(&low_west_queue, train->id);
            queueSizes[3]--;
        }
    }

    // Train departs
    pthread_mutex_lock(&clock_mutex);
    // occupy the track
    track_occupied = 1;
    // print "ON the track" when the train gets on
    print_time(global_time);
    printf(" Train %2d is ON the main track going %s\n", train->id, get_direction_string(train->direction));
    pthread_mutex_unlock(&clock_mutex);
    pthread_mutex_unlock(&track_mutex);

    // simulate the train crossing the track
    int crossing_end_time = global_time + train->crossing_time;
    while (true) {
        pthread_mutex_lock(&clock_mutex);
        // break if finished crossing
        if (global_time >= crossing_end_time) {
            pthread_mutex_unlock(&clock_mutex);
            break;
        }
        // keep crossing
        pthread_mutex_unlock(&clock_mutex);
        usleep(TIME_INCREMENT);
    }

    // get any direction requirements for next train
    // if queue has trains of both directions, pick opposite direction of the last train
    if ((queueSizes[0] > 0 && queueSizes[1] > 0) || 
            (queueSizes[0] == 0 && queueSizes[1] == 0 && queueSizes[2] > 0 && queueSizes[3] > 0)) {
        if (train->direction == 'e') {
            next_dir = 'w';
        } else {
            next_dir = 'e';
        }
    } else {
        next_dir = 'b';
    }

    // starvation prevention
    if (train->direction == curr_dir) {
        if (train->direction == 'e') {
            next_dir = 'w';
        } else {
            next_dir = 'e';
        }
    }
    curr_dir = train->direction;

    // find the next train that's ready for departure
    findHighestPrioTrain();

    // train is done crossing, release the track
    pthread_mutex_lock(&track_mutex);
    track_occupied = 0;

    // print "OFF the track" when the train gets off
    print_time(global_time);
    printf(" Train %2d is OFF the main track after going %s\n", train->id, get_direction_string(train->direction));
    pthread_mutex_unlock(&clock_mutex);

    // Signal the next train that the track is free
    pthread_cond_broadcast(&track_cond);
    pthread_mutex_unlock(&track_mutex);

    // free train memory
    free(train);
    return NULL;
}

// main function
int main(int argc, char *argv[]) {
    // array of trains with their data
    Train trains[75];
    int num_trains;

    // initialize trains from file
    initTrains(argv[1], trains, &num_trains);

    // initialize clock mutex
    pthread_t clock_tid;
    pthread_create(&clock_tid, NULL, clock_thread, NULL);
    
    // create train threads
    pthread_t train_tids[num_trains];
    for (int i = 0; i < num_trains; i++) {
        // allocate memory
        Train* train = (Train*) malloc(sizeof(Train));
        *train = trains[i];
        pthread_create(&train_tids[i], NULL, train_thread, train);
    }
    
    // join threads after they finish execution
    for (int i = 0; i < num_trains; i++) {
        pthread_join(train_tids[i], NULL);
    }
    
    // cancel and destroy track and clock mutexes
    pthread_cancel(clock_tid);
    pthread_mutex_destroy(&track_mutex);
    pthread_cond_destroy(&track_cond);
    pthread_mutex_destroy(&clock_mutex);

    return 0;
}