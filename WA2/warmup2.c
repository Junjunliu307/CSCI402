#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <stdint.h>  
#include <signal.h>
#include "my402list.h"

double total_inter_arrival_time = 0.0;
double total_service_time = 0.0;
double total_time_in_Q1 = 0.0;
double total_time_in_Q2 = 0.0;
double total_time_in_S1 = 0.0;
double total_time_in_S2 = 0.0;
double total_time_in_system = 0.0;
double total_system_time_squared = 0.0;

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
double mu = 0.3;
double r = 1.5;
int B = 10;
int P = 3;
int num_packets = 20;
int deterministic_mode = 1;  // Default is deterministic mode
char tracefile[256] = {0};

// Emulation start time
struct timeval emulation_start;
struct timeval emulation_end;
int is_emulation_finished = 0;

// Function prototypes
void *packet_arrival_thread(void *param);
void *token_deposit_thread(void *param);
void *server_thread(void *param);
void parse_trace_file(const char *filename);
void *parse_trace_file_thread(void *param);
double get_elapsed_time_in_ms(struct timeval *start, struct timeval *end);
void print_statistics();

void *sigint_handler_thread(void *arg);
void remove_packets_from_queue(My402List *queue);

// Thread
pthread_t packet_thread, token_thread, server1, server2, sigint_thread;
sigset_t set;

int main(int argc, char *argv[]) {
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-lambda") == 0 && i + 1 < argc) {
            lambda = atof(argv[i + 1]);
            i++; 
        } else if (strcmp(argv[i], "-mu") == 0 && i + 1 < argc) {
            mu = atof(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            r = atof(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-B") == 0 && i + 1 < argc) {
            B = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-P") == 0 && i + 1 < argc) {
            P = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            num_packets = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            strncpy(tracefile, argv[i + 1], sizeof(tracefile) - 1);
            deterministic_mode = 0;  // Switch to trace-driven mode
            i++;
        } else {
            fprintf(stderr, "Usage: %s [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // 初始化队列
    if (My402ListInit(&Q1) != 1 || My402ListInit(&Q2) != 1) {
        fprintf(stderr, "Failed to initialize queues\n");
        exit(EXIT_FAILURE);
    }

    // 获取仿真开始时间
    gettimeofday(&emulation_start, NULL);

    // 解析 trace 文件（如果有）
    if (!deterministic_mode) {
        parse_trace_file(tracefile);
    }

    // 打印仿真参数
    printf("Emulation Parameters:\n");
    printf("    number to arrive = %d\n", num_packets);
    if (deterministic_mode) {
        printf("    lambda = %.6g\n", lambda);
        printf("    mu = %.6g\n", mu);
    }
    printf("    r = %.6g\n", r);
    printf("    B = %d\n", B);
    printf("    P = %d\n", P);
    if (!deterministic_mode) {
        printf("    tsfile = %s\n", tracefile);
    }

    printf("%08d.%03dms: emulation begins\n", 0, 0);

    // 设置信号屏蔽集以包含 SIGINT
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        perror("pthread_sigmask failed");
        exit(1);
    }

    // 创建 SIGINT 捕获线程
    if (pthread_create(&sigint_thread, NULL, sigint_handler_thread, (void*)&set) != 0) {
        perror("Failed to create SIGINT handler thread");
        exit(EXIT_FAILURE);
    }

    // 创建其他线程
    // if (pthread_create(&packet_thread, NULL, packet_arrival_thread, NULL) != 0) {
    //     perror("Failed to create packet thread");
    //     exit(EXIT_FAILURE);
    // }
    if (deterministic_mode) {
        if (pthread_create(&packet_thread, NULL, packet_arrival_thread, NULL) != 0) {
            perror("Failed to create packet thread");
            exit(EXIT_FAILURE);
        }
    } else {
        if (pthread_create(&packet_thread, NULL, parse_trace_file_thread, (void*)tracefile) != 0) {
            perror("Failed to create parse trace file thread");
            exit(EXIT_FAILURE);
        }
    }

    if (pthread_create(&token_thread, NULL, token_deposit_thread, NULL) != 0) {
        perror("Failed to create token thread");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&server1, NULL, server_thread, (void *)(intptr_t)1) != 0) {
        perror("Failed to create server 1 thread");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&server2, NULL, server_thread, (void *)(intptr_t)2) != 0) {
        perror("Failed to create server 2 thread");
        exit(EXIT_FAILURE);
    }

    // 等待数据包到达线程完成
    pthread_join(packet_thread, NULL);

    // 数据包到达线程完成后标记仿真结束
    pthread_mutex_lock(&queue_lock);
    is_emulation_finished = 1; 
    pthread_mutex_unlock(&queue_lock);

    // 等待其他线程完成
    pthread_join(token_thread, NULL);
    pthread_join(server1, NULL);
    pthread_join(server2, NULL);
    // pthread_join(sigint_thread, NULL);

    // 获取仿真结束时间并输出结束信息
    gettimeofday(&emulation_end, NULL);
    double termination_time_ms = get_elapsed_time_in_ms(&emulation_start, &emulation_end);
    printf("%012.3fms: emulation ends\n", termination_time_ms);

    // 打印统计信息
    print_statistics();

    // 清理资源
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
        packet->service_time_ms = 1000 / mu;
        gettimeofday(&packet->arrival_time, NULL);

        double inter_arrival_time = get_elapsed_time_in_ms(&last_arrival_time, &packet->arrival_time);
        last_arrival_time = packet->arrival_time;

        total_inter_arrival_time += inter_arrival_time;
        total_packets_arrived++;

        if (packet->tokens_needed > B) {
            total_packets_dropped++;
            printf("%012.3fms: p%d arrives, needs %d tokens, inter-arrival time = %.6gms, dropped\n",
                   get_elapsed_time_in_ms(&emulation_start, &packet->arrival_time),
                   packet->id, packet->tokens_needed, inter_arrival_time);
            free(packet);
            continue;
        }
        printf("%012.3fms: p%d arrives, needs %d tokens, inter-arrival time = %.3fms\n",
               get_elapsed_time_in_ms(&emulation_start, &packet->arrival_time), i, P, inter_arrival_time);

        pthread_mutex_lock(&queue_lock);
        if (My402ListAppend(&Q1, packet) != 1) {
            fprintf(stderr, "Failed to append packet to Q1\n");
            pthread_mutex_unlock(&queue_lock);
            exit(EXIT_FAILURE);
        }
        printf("%012.3fms: p%d enters Q1\n", get_elapsed_time_in_ms(&emulation_start, &packet->arrival_time), i);
        pthread_mutex_unlock(&queue_lock);
    }
    return NULL;
}

void *token_deposit_thread(void *param) {
    int token_id = 1;
    while (!is_emulation_finished || !My402ListEmpty(&Q1)) {
        usleep((useconds_t)(1.0 / r * 1000000));

        pthread_mutex_lock(&token_lock);
        total_tokens_generated++;
        if (tokens < B) {
            tokens++;
            struct timeval token_arrival_time;
            gettimeofday(&token_arrival_time, NULL);
            printf("%012.3fms: token t%d arrives, token bucket now has %d %s\n",
                get_elapsed_time_in_ms(&emulation_start, &token_arrival_time), token_id++, tokens,
                tokens > 1 ? "tokens" : "token");
        } else {
            struct timeval token_arrival_time;
            gettimeofday(&token_arrival_time, NULL);
            printf("%012.3fms: token t%d arrives, dropped\n",
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
                total_time_in_Q1 += get_elapsed_time_in_ms(&packet->arrival_time, &leave_Q1_time);

                if (My402ListAppend(&Q2, packet) != 1) {
                    fprintf(stderr, "Failed to append packet to Q2\n");
                    pthread_mutex_unlock(&queue_lock);
                    exit(EXIT_FAILURE);
                }
                printf("%012.3fms: p%d leaves Q1, time in Q1 = %.3fms, token bucket now has %d %s\n",
                    get_elapsed_time_in_ms(&emulation_start, &leave_Q1_time),
                    packet->id, get_elapsed_time_in_ms(&packet->arrival_time, &leave_Q1_time),
                    tokens, tokens > 1 ? "tokens" : "token");

                gettimeofday(&packet->enter_Q2_time, NULL);
                printf("%012.3fms: p%d enters Q2\n", get_elapsed_time_in_ms(&emulation_start, &packet->enter_Q2_time), packet->id);
            } else {
                // total_packets_dropped++;
                break;
            }
        }
        pthread_mutex_unlock(&queue_lock);
    }
    return NULL;
}

void *server_thread(void *param) {
    intptr_t server_id = (intptr_t)param;
    while (!is_emulation_finished || !My402ListEmpty(&Q2) || !My402ListEmpty(&Q1)) {
        pthread_mutex_lock(&queue_lock);
        if (!My402ListEmpty(&Q2)) {
            My402ListElem *elem = My402ListFirst(&Q2);
            Packet *packet = (Packet *)elem->obj;
            My402ListUnlink(&Q2, elem);

            struct timeval leave_Q2_time;
            gettimeofday(&leave_Q2_time, NULL);

            total_time_in_Q2 += get_elapsed_time_in_ms(&packet->enter_Q2_time, &leave_Q2_time);
            printf("%012.3fms: p%d leaves Q2, time in Q2 = %.3fms\n",
                get_elapsed_time_in_ms(&emulation_start, &leave_Q2_time),
                packet->id,
                get_elapsed_time_in_ms(&packet->enter_Q2_time, &leave_Q2_time));
            struct timeval begin_service_time;
            gettimeofday(&begin_service_time, NULL);
            printf("%012.3fms: p%d begins service at S%ld, requesting %dms of service\n",
                   get_elapsed_time_in_ms(&emulation_start, &begin_service_time),
                   packet->id, (long)server_id, packet->service_time_ms);

            pthread_mutex_unlock(&queue_lock);

            usleep(packet->service_time_ms * 1000);
            struct timeval depart_time;
            gettimeofday(&depart_time, NULL);

            double time_in_system = get_elapsed_time_in_ms(&packet->arrival_time, &depart_time);
            total_time_in_system += time_in_system;
            total_system_time_squared += time_in_system * time_in_system;

            total_packets_served ++;
            total_service_time += get_elapsed_time_in_ms(&begin_service_time, &depart_time);
            if (server_id == 1){
                total_time_in_S1 += get_elapsed_time_in_ms(&begin_service_time, &depart_time);
            }else{
                total_time_in_S2 += get_elapsed_time_in_ms(&begin_service_time, &depart_time);
            }
            printf("%012.3fms: p%d departs from S%ld, service time = %.3fms, time in system = %.3fms\n",
                   get_elapsed_time_in_ms(&emulation_start, &depart_time),
                   packet->id, (long)server_id,
                   get_elapsed_time_in_ms(&begin_service_time, &depart_time),
                   get_elapsed_time_in_ms(&packet->arrival_time, &depart_time));

            free(packet);
        } else {
            pthread_mutex_unlock(&queue_lock);
            usleep(1000);
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
    num_packets = num_packets_from_trace;

    // for (int i = 1; i <= num_packets_from_trace; i++) {
    //     int inter_arrival_time_ms, tokens_needed, service_time_ms;
    //     if (fscanf(file, "%d%d%d", &inter_arrival_time_ms, &tokens_needed, &service_time_ms) != 3) {
    //         fprintf(stderr, "Error reading trace file on line %d\n", i + 1);
    //         exit(EXIT_FAILURE);
    //     }

    //     usleep(inter_arrival_time_ms * 1000);

    //     Packet *packet = (Packet *)malloc(sizeof(Packet));
    //     packet->id = i;
    //     packet->tokens_needed = tokens_needed;
    //     packet->service_time_ms = service_time_ms;
    //     gettimeofday(&packet->arrival_time, NULL);

    //     pthread_mutex_lock(&queue_lock);
    //     My402ListAppend(&Q1, packet);
    //     pthread_mutex_unlock(&queue_lock);
    // }

    fclose(file);
}

void *parse_trace_file_thread(void *param) {
    const char *filename = (const char *)param;

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening trace file");
        exit(EXIT_FAILURE);
    }

    int num_packets_from_trace;
    if (fscanf(file, "%d", &num_packets_from_trace) != 1) {
        fprintf(stderr, "Error reading the number of packets from trace file\n");
        exit(EXIT_FAILURE);
    }
    num_packets = num_packets_from_trace;

    struct timeval last_arrival_time = emulation_start;

    for (int i = 1; i <= num_packets_from_trace; i++) {
        int inter_arrival_time_ms, tokens_needed, service_time_ms;
        if (fscanf(file, "%d%d%d", &inter_arrival_time_ms, &tokens_needed, &service_time_ms) != 3) {
            fprintf(stderr, "Error reading trace file on line %d\n", i + 1);
            exit(EXIT_FAILURE);
        }

        // 模拟数据包的到达间隔
        usleep(inter_arrival_time_ms * 1000);

        Packet *packet = (Packet *)malloc(sizeof(Packet));
        if (!packet) {
            fprintf(stderr, "Memory allocation failed for packet %d\n", i);
            exit(EXIT_FAILURE);
        }
        packet->id = i;
        packet->tokens_needed = tokens_needed;
        packet->service_time_ms = service_time_ms;
        gettimeofday(&packet->arrival_time, NULL);

        double inter_arrival_time = get_elapsed_time_in_ms(&last_arrival_time, &packet->arrival_time);
        last_arrival_time = packet->arrival_time;

        total_inter_arrival_time += inter_arrival_time;
        total_packets_arrived++;

        // 检查数据包所需的令牌数量是否超过桶的最大容量 B
        if (packet->tokens_needed > B) {
            total_packets_dropped++;
            printf("%012.3fms: p%d arrives, needs %d tokens, inter-arrival time = %.6gms, dropped\n",
                   get_elapsed_time_in_ms(&emulation_start, &packet->arrival_time),
                   packet->id, packet->tokens_needed, inter_arrival_time);
            free(packet);
            continue;
        }

        printf("%012.3fms: p%d arrives, needs %d tokens, inter-arrival time = %.3fms\n",
               get_elapsed_time_in_ms(&emulation_start, &packet->arrival_time), i, tokens_needed, inter_arrival_time);

        // 插入到 Q1 队列
        pthread_mutex_lock(&queue_lock);
        if (My402ListAppend(&Q1, packet) != 1) {
            fprintf(stderr, "Failed to append packet to Q1\n");
            pthread_mutex_unlock(&queue_lock);
            exit(EXIT_FAILURE);
        }
        printf("%012.3fms: p%d enters Q1\n", get_elapsed_time_in_ms(&emulation_start, &packet->arrival_time), i);
        pthread_mutex_unlock(&queue_lock);
    }

    fclose(file);
    return NULL;
}

void print_statistics() {
    printf("\nStatistics:\n");

    if (total_packets_arrived > 1) {
        printf("\taverage packet inter-arrival time = %.6g\n", (total_inter_arrival_time / (total_packets_arrived)) / 1000.0);
    } else {
        printf("\taverage packet inter-arrival time = N/A (no packets arrived)\n");
    }

    if (total_packets_served > 0) {
        printf("\taverage packet service time = %.6g\n", (total_service_time / total_packets_served) / 1000.0);
    } else {
        printf("\taverage packet service time = N/A (no packets served)\n");
    }

    gettimeofday(&emulation_end, NULL);
    double emulation_time = get_elapsed_time_in_ms(&emulation_start, &emulation_end) / 1000.0;

    printf("\taverage number of packets in Q1 = %.6g\n", total_time_in_Q1 / (emulation_time * 1000.0));
    printf("\taverage number of packets in Q2 = %.6g\n", total_time_in_Q2 / (emulation_time * 1000.0));
    printf("\taverage number of packets at S1 = %.6g\n", total_time_in_S1 / (emulation_time * 1000.0));
    printf("\taverage number of packets at S2 = %.6g\n", total_time_in_S2 / (emulation_time * 1000.0));

    if (total_packets_served > 0) {
        double avg_time_in_system = (total_time_in_system / total_packets_served) / 1000.0;
        double variance = ((total_system_time_squared / total_packets_served) / 1000000.0) - (avg_time_in_system * avg_time_in_system);
        double stddev = sqrt(variance);
        printf("\taverage time a packet spent in system = %.6g\n", avg_time_in_system);
        printf("\tstandard deviation for time spent in system = %.6g\n", stddev);
    } else {
        printf("\taverage time a packet spent in system = N/A (no packets served)\n");
        printf("\tstandard deviation for time spent in system = N/A\n");
    }

    if (total_tokens_generated > 0) {
        printf("\ttoken drop probability = %.6g\n", (double)total_tokens_dropped / total_tokens_generated);
    } else {
        printf("\ttoken drop probability = N/A (no tokens generated)\n");
    }

    if (total_packets_arrived > 0) {
        printf("\tpacket drop probability = %.6g\n", (double)total_packets_dropped / total_packets_arrived);
    } else {
        printf("\tpacket drop probability = N/A (no packets arrived)\n");
    }
}


double get_elapsed_time_in_ms(struct timeval *start, struct timeval *end) {
    return (double)(end->tv_sec - start->tv_sec) * 1000.0 + (double)(end->tv_usec - start->tv_usec) / 1000.0;
}

// 信号捕获线程函数
void* sigint_handler_thread(void* arg) {
    int sig;

    // 使用 sigwait 等待 SIGINT 信号
    if (sigwait(&set, &sig) != 0) {
        perror("sigwait failed");
        exit(1);
    }

    // SIGINT 被捕获，开始处理终止逻辑
    printf("\nSIGINT caught, no new packets or tokens will be allowed\n");

    // 标记仿真结束
    is_emulation_finished = 1;

    // 停止数据包到达线程和令牌生成线程
    pthread_cancel(packet_thread);
    pthread_cancel(token_thread);

    // 清空 Q1 和 Q2 中的数据包
    pthread_mutex_lock(&queue_lock);
    remove_packets_from_queue(&Q1);
    remove_packets_from_queue(&Q2);
    pthread_mutex_unlock(&queue_lock);

    // 等待服务器线程完成当前服务的数据包
    pthread_cancel(server1);
    pthread_cancel(server2);

    // 输出统计信息
    print_statistics();

    // 退出程序
    exit(0);
}

// 清空队列并打印信息的函数
void remove_packets_from_queue(My402List* queue) {
    while (!My402ListEmpty(queue)) {
        My402ListElem *elem = My402ListFirst(queue);
        Packet *packet = (Packet *)elem->obj;
        My402ListUnlink(queue, elem);
        printf("p%d removed from queue\n", packet->id);
        free(packet);
    }
}