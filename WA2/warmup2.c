#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <stdint.h>  
#include "my402list.h"

#define MAX_PACKETS 10000  // Example limit

double total_inter_arrival_time = 0.0;
double total_service_time = 0.0;
double total_time_in_Q1 = 0.0;
double total_time_in_Q2 = 0.0;
double total_time_in_S1 = 0.0;
double total_time_in_S2 = 0.0;
double total_time_in_system = 0.0;
double total_system_time_squared = 0.0;  // 用于计算系统内时间的方差

int total_packets_arrived = 0;
int total_packets_served = 0;
int total_packets_dropped = 0;
int total_tokens_generated = 0;
int total_tokens_dropped = 0;

// Structure to represent packets
typedef struct Packet {
    int id;
    int tokens_needed;
    int service_time_ms;
    struct timeval arrival_time;
    struct timeval enter_Q1_time;
    struct timeval leave_Q1_time;
    struct timeval enter_Q2_time;
    struct timeval leave_Q2_time;
    struct timeval begin_service_time;
    struct timeval depart_time;
} Packet;

// Token bucket parameters
int bucket_capacity = 10;
int tokens = 0;
pthread_mutex_t token_lock = PTHREAD_MUTEX_INITIALIZER;

// Queues for Q1 and Q2
My402List Q1, Q2;
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;

// Global parameters
double lambda = 1.0;
double mu = 0.35;
double r = 1.5;
int B = 10;
int P = 3;
int num_packets = 20;
int deterministic_mode = 1;  // Default is deterministic mode
char tracefile[256] = {0};

// Emulation start time
struct timeval emulation_start;
int is_emulation_finished = 0;  // 标志仿真是否结束

// Function prototypes
void *packet_arrival_thread(void *param);
void *token_deposit_thread(void *param);
void *server_thread(void *param);
void parse_trace_file(const char *filename);
double get_elapsed_time_in_ms(struct timeval *start, struct timeval *end);
void print_statistics();

int main(int argc, char *argv[]) {
    int opt;

    // Command-line parsing
    while ((opt = getopt(argc, argv, "l:m:r:B:P:n:t:")) != -1) {
        switch (opt) {
            case 'l':
                lambda = atof(optarg);
                break;
            case 'm':
                mu = atof(optarg);
                break;
            case 'r':
                r = atof(optarg);
                break;
            case 'B':
                B = atoi(optarg);
                break;
            case 'P':
                P = atoi(optarg);
                break;
            case 'n':
                num_packets = atoi(optarg);
                break;
            case 't':
                strncpy(tracefile, optarg, sizeof(tracefile) - 1);
                deterministic_mode = 0;  // Switch to trace-driven mode
                break;
            default:
                fprintf(stderr, "Usage: %s [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Initialize queues
    if (My402ListInit(&Q1) != 1 || My402ListInit(&Q2) != 1) {
        fprintf(stderr, "Failed to initialize queues\n");
        exit(EXIT_FAILURE);
    }

    // Get the start time for the emulation
    gettimeofday(&emulation_start, NULL);

    // Print emulation parameters
    printf("Emulation Parameters:\n");
    printf("number to arrive = %d\n", num_packets);
    if (deterministic_mode) {
        printf("lambda = %.6g\n", lambda);
        printf("mu = %.6g\n", mu);
    }
    printf("r = %.6g\n", r);
    printf("B = %d\n", B);
    printf("P = %d\n", P);
    if (!deterministic_mode) {
        printf("tsfile = %s\n", tracefile);
    }

    // If trace file is provided, parse it
    if (!deterministic_mode) {
        parse_trace_file(tracefile);
    }

    printf("%08d.%03dms: emulation begins\n", 0, 0);

    pthread_t packet_thread, token_thread, server1, server2;

    // Create threads
    if (pthread_create(&packet_thread, NULL, packet_arrival_thread, NULL) != 0) {
        perror("Failed to create packet thread");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&token_thread, NULL, token_deposit_thread, NULL) != 0) {
        perror("Failed to create token thread");
        exit(EXIT_FAILURE);
    }

    // 使用 (void *)(intptr_t) 来传递整数值
    if (pthread_create(&server1, NULL, server_thread, (void *)(intptr_t)1) != 0) {
        perror("Failed to create server 1 thread");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&server2, NULL, server_thread, (void *)(intptr_t)2) != 0) {
        perror("Failed to create server 2 thread");
        exit(EXIT_FAILURE);
    }

    // Wait for packet thread to complete (all packets are generated)
    pthread_join(packet_thread, NULL);

    // After packet thread completes, mark emulation as finished
    pthread_mutex_lock(&queue_lock);
    is_emulation_finished = 1;
    pthread_mutex_unlock(&queue_lock);

    // Wait for the other threads to complete
    pthread_join(token_thread, NULL);
    pthread_join(server1, NULL);
    pthread_join(server2, NULL);

    printf("%08d.%03dms: emulation ends\n", 0, 0);

    // 打印统计信息
    print_statistics();

    // Cleanup
    pthread_mutex_destroy(&token_lock);
    pthread_mutex_destroy(&queue_lock);
    My402ListUnlinkAll(&Q1);
    My402ListUnlinkAll(&Q2);

    return 0;
}

void *packet_arrival_thread(void *param) {
    struct timeval last_arrival_time = emulation_start;
    for (int i = 1; i <= num_packets; i++) {
        usleep((useconds_t)(1.0 / lambda * 1000000));

        Packet *packet = (Packet *)malloc(sizeof(Packet));
        if (!packet) {
            fprintf(stderr, "Memory allocation failed for packet %d\n", i);
            exit(EXIT_FAILURE);
        }
        packet->id = i;
        packet->tokens_needed = P;
        gettimeofday(&packet->arrival_time, NULL);

        double inter_arrival_time = get_elapsed_time_in_ms(&last_arrival_time, &packet->arrival_time);
        last_arrival_time = packet->arrival_time;

        total_inter_arrival_time += inter_arrival_time;  // 收集到达时间
        total_packets_arrived++;

        printf("%08.3fms: p%d arrives, needs %d tokens, inter-arrival time = %.3fms\n",
               get_elapsed_time_in_ms(&emulation_start, &packet->arrival_time), i, P, inter_arrival_time);

        pthread_mutex_lock(&queue_lock);
        if (My402ListAppend(&Q1, packet) != 1) {
            fprintf(stderr, "Failed to append packet to Q1\n");
            pthread_mutex_unlock(&queue_lock);
            exit(EXIT_FAILURE);
        }
        printf("%08.3fms: p%d enters Q1\n", get_elapsed_time_in_ms(&emulation_start, &packet->arrival_time), i);
        pthread_mutex_unlock(&queue_lock);
    }
    return NULL;
}

void *token_deposit_thread(void *param) {
    int token_id = 1;
    while (!is_emulation_finished) {
        usleep((useconds_t)(1.0 / r * 1000000));

        pthread_mutex_lock(&token_lock);
        total_tokens_generated++;
        if (tokens < B) {
            tokens++;
            struct timeval token_arrival_time;
            gettimeofday(&token_arrival_time, NULL);
            printf("%08.3fms: token t%d arrives, token bucket now has %d tokens\n",
                   get_elapsed_time_in_ms(&emulation_start, &token_arrival_time), token_id++, tokens);
        } else {
            struct timeval token_arrival_time;
            gettimeofday(&token_arrival_time, NULL);
            printf("%08.3fms: token t%d arrives, dropped\n",
                   get_elapsed_time_in_ms(&emulation_start, &token_arrival_time), token_id++);
            total_tokens_dropped++;
        }
        pthread_mutex_unlock(&token_lock);

        // Process packets in Q1 if they can be moved to Q2
        pthread_mutex_lock(&queue_lock);
        while (!My402ListEmpty(&Q1)) {
            My402ListElem *elem = My402ListFirst(&Q1);
            Packet *packet = (Packet *)elem->obj;
            if (packet->tokens_needed <= tokens) {
                tokens -= packet->tokens_needed;
                struct timeval leave_Q1_time;
                gettimeofday(&leave_Q1_time, NULL);
                My402ListUnlink(&Q1, elem);
                if (My402ListAppend(&Q2, packet) != 1) {
                    fprintf(stderr, "Failed to append packet to Q2\n");
                    pthread_mutex_unlock(&queue_lock);
                    exit(EXIT_FAILURE);
                }
                total_time_in_Q1 += get_elapsed_time_in_ms(&packet->arrival_time, &leave_Q1_time);  // 记录在Q1的时间
                printf("%08.3fms: p%d leaves Q1, time in Q1 = %.3fms, token bucket now has %d tokens\n",
                       get_elapsed_time_in_ms(&emulation_start, &leave_Q1_time),
                       packet->id, get_elapsed_time_in_ms(&packet->arrival_time, &leave_Q1_time), tokens);
                printf("%08.3fms: p%d enters Q2\n", get_elapsed_time_in_ms(&emulation_start, &leave_Q1_time), packet->id);
            } else {
                total_packets_dropped++;
                break;
            }
        }
        pthread_mutex_unlock(&queue_lock);
    }
    return NULL;
}

void *server_thread(void *param) {
    intptr_t server_id = (intptr_t)param;
    while (!is_emulation_finished || !My402ListEmpty(&Q2)) {
        pthread_mutex_lock(&queue_lock);
        if (!My402ListEmpty(&Q2)) {
            My402ListElem *elem = My402ListFirst(&Q2);
            Packet *packet = (Packet *)elem->obj;
            My402ListUnlink(&Q2, elem);

            struct timeval begin_service_time;
            gettimeofday(&begin_service_time, NULL);
            printf("%08.3fms: p%d begins service at S%ld, requesting %dms of service\n",
                   get_elapsed_time_in_ms(&emulation_start, &begin_service_time),
                   packet->id, (long)server_id, packet->service_time_ms);

            if (server_id == 1) {
                total_time_in_S1 += get_elapsed_time_in_ms(&begin_service_time, &packet->depart_time);
            } else {
                total_time_in_S2 += get_elapsed_time_in_ms(&begin_service_time, &packet->depart_time);
            }

            pthread_mutex_unlock(&queue_lock);

            usleep(packet->service_time_ms * 1000);
            struct timeval depart_time;
            gettimeofday(&depart_time, NULL);
            printf("%08.3fms: p%d departs from S%ld, service time = %.3fms, time in system = %.3fms\n",
                   get_elapsed_time_in_ms(&emulation_start, &depart_time),
                   packet->id, (long)server_id,
                   get_elapsed_time_in_ms(&begin_service_time, &depart_time),
                   get_elapsed_time_in_ms(&packet->arrival_time, &depart_time));

            double time_in_system = get_elapsed_time_in_ms(&packet->arrival_time, &depart_time);
            total_time_in_system += time_in_system;
            total_system_time_squared += time_in_system * time_in_system;

            total_service_time += get_elapsed_time_in_ms(&begin_service_time, &depart_time);  // 记录服务时间
            total_packets_served++;
            free(packet);
        } else {
            pthread_mutex_unlock(&queue_lock);
            usleep(1000);  // Sleep briefly if no packets to serve
        }
    }
    return NULL;
}
// Parse the trace file for trace-driven mode
void parse_trace_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening trace file");
        exit(EXIT_FAILURE);
    }

    int num_packets_from_trace;
    fscanf(file, "%d", &num_packets_from_trace);

    for (int i = 1; i <= num_packets_from_trace; i++) {
        int inter_arrival_time_ms, tokens_needed, service_time_ms;
        if (fscanf(file, "%d%d%d", &inter_arrival_time_ms, &tokens_needed, &service_time_ms) != 3) {
            fprintf(stderr, "Error reading trace file on line %d\n", i + 1);
            exit(EXIT_FAILURE);
        }

        usleep(inter_arrival_time_ms * 1000);

        Packet *packet = (Packet *)malloc(sizeof(Packet));
        packet->id = i;
        packet->tokens_needed = tokens_needed;
        packet->service_time_ms = service_time_ms;
        gettimeofday(&packet->arrival_time, NULL);

        pthread_mutex_lock(&queue_lock);
        My402ListAppend(&Q1, packet);
        pthread_mutex_unlock(&queue_lock);
    }

    fclose(file);
}

// 打印统计信息
void print_statistics() {
    printf("\nStatistics:\n");

    // 平均包到达时间
    if (total_packets_arrived > 1) {
        printf("\taverage packet inter-arrival time = %.6g\n", total_inter_arrival_time / (total_packets_arrived - 1));
    } else {
        printf("\taverage packet inter-arrival time = N/A (no packets arrived)\n");
    }

    // 平均包服务时间
    if (total_packets_served > 0) {
        printf("\taverage packet service time = %.6g\n", total_service_time / total_packets_served);
    } else {
        printf("\taverage packet service time = N/A (no packets served)\n");
    }

    // 平均Q1、Q2、S1、S2中的包数
    double emulation_time = get_elapsed_time_in_ms(&emulation_start, &(struct timeval){0});
    printf("\taverage number of packets in Q1 = %.6g\n", total_time_in_Q1 / emulation_time);
    printf("\taverage number of packets in Q2 = %.6g\n", total_time_in_Q2 / emulation_time);
    printf("\taverage number of packets at S1 = %.6g\n", total_time_in_S1 / emulation_time);
    printf("\taverage number of packets at S2 = %.6g\n", total_time_in_S2 / emulation_time);

    // 包在系统中的平均时间和标准差
    if (total_packets_served > 0) {
        double avg_time_in_system = total_time_in_system / total_packets_served;
        double variance = (total_system_time_squared / total_packets_served) - (avg_time_in_system * avg_time_in_system);
        double stddev = sqrt(variance);
        printf("\taverage time a packet spent in system = %.6g\n", avg_time_in_system);
        printf("\tstandard deviation for time spent in system = %.6g\n", stddev);
    } else {
        printf("\taverage time a packet spent in system = N/A (no packets served)\n");
        printf("\tstandard deviation for time spent in system = N/A\n");
    }

    // 令牌丢弃概率
    if (total_tokens_generated > 0) {
        printf("\ttoken drop probability = %.6g\n", (double)total_tokens_dropped / total_tokens_generated);
    } else {
        printf("\ttoken drop probability = N/A (no tokens generated)\n");
    }

    // 包丢弃概率
    if (total_packets_arrived > 0) {
        printf("\tpacket drop probability = %.6g\n", (double)total_packets_dropped / total_packets_arrived);
    } else {
        printf("\tpacket drop probability = N/A (no packets arrived)\n");
    }
}

// 计算时间间隔
double get_elapsed_time_in_ms(struct timeval *start, struct timeval *end) {
    return (double)(end->tv_sec - start->tv_sec) * 1000.0 + (double)(end->tv_usec - start->tv_usec) / 1000.0;
}
