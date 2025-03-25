#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>

#define MAX_PACKET_SIZE 65507  // Maximum UDP packet size

typedef struct {
    uint32_t seq_num;          // Sequence number
    uint32_t timestamp_sec;    // Timestamp seconds
    uint32_t timestamp_usec;   // Timestamp microseconds
    char data[1];              // Data starts here
} packet_t;

int running = 1;
unsigned long long total_bytes = 0;
unsigned long long total_packets = 0;
unsigned long long lost_packets = 0;
uint32_t last_seq_num = 0;

void signal_handler(int sig) {
    running = 0;
}

void print_statistics(struct timeval *start_time, struct timeval *end_time) {
    double duration = (end_time->tv_sec - start_time->tv_sec) +
                     (end_time->tv_usec - start_time->tv_usec) / 1000000.0;

    double throughput_mbps = (total_bytes * 8.0) / (duration * 1000000.0);
    double packet_loss = 0.0;

    if (total_packets > 0 && last_seq_num > 0) {
        lost_packets = (last_seq_num + 1) - total_packets;
        packet_loss = (double)lost_packets / (last_seq_num + 1) * 100.0;
    }

    printf("测试结果:\n");
    printf("  接收总字节数: %llu 字节\n", total_bytes);
    printf("  接收总包数: %llu 包\n", total_packets);
    printf("  丢包数: %llu 包\n", lost_packets);
    printf("  丢包率: %.2f%%\n", packet_loss);
    printf("  测试持续时间: %.2f 秒\n", duration);
    printf("  吞吐量: %.2f Mbps\n", throughput_mbps);
}

int main(int argc, char *argv[]) {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char *buffer;
    int port = 8888;
    int opt;
    struct timeval start_time, end_time;

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "用法: %s [-p port]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Allocate buffer for receiving packets
    buffer = (char *)malloc(MAX_PACKET_SIZE);
    if (!buffer) {
        perror("内存分配失败");
        exit(EXIT_FAILURE);
    }

    // Create UDP socket
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("套接字创建失败");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket to address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("绑定失败");
        exit(EXIT_FAILURE);
    }

    printf("UDP服务器已启动，监听端口 %d...\n", port);

    // Set up signal handler for graceful termination
    signal(SIGINT, signal_handler);

    // Wait for the first packet to start timing
    int recv_len = recvfrom(server_fd, buffer, MAX_PACKET_SIZE, 0,
                           (struct sockaddr *)&client_addr, &client_len);

    if (recv_len > 0) {
        packet_t *packet = (packet_t *)buffer;
        last_seq_num = ntohl(packet->seq_num);
        total_packets = 1;
        total_bytes = recv_len;

        gettimeofday(&start_time, NULL);
        printf("开始接收数据包，第一个序列号: %u\n", last_seq_num);
    }

    // Main receive loop
    while (running) {
        int recv_len = recvfrom(server_fd, buffer, MAX_PACKET_SIZE, 0,
                               (struct sockaddr *)&client_addr, &client_len);

        if (recv_len > 0) {
            packet_t *packet = (packet_t *)buffer;
            uint32_t seq_num = ntohl(packet->seq_num);

            // Check for packet loss
            if (seq_num != last_seq_num + 1 && last_seq_num != 0) {
                printf("检测到丢包: 预期 %u, 收到 %u\n", last_seq_num + 1, seq_num);
            }

            last_seq_num = seq_num;
            total_packets++;
            total_bytes += recv_len;
        }
    }

    gettimeofday(&end_time, NULL);
    print_statistics(&start_time, &end_time);

    free(buffer);
    close(server_fd);
    return 0;
}