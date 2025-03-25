#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>

typedef struct {
    uint32_t seq_num;          // Sequence number
    uint32_t timestamp_sec;    // Timestamp seconds
    uint32_t timestamp_usec;   // Timestamp microseconds
    char data[1];              // Data starts here
} packet_t;

int running = 1;
unsigned long long total_bytes = 0;
unsigned long long total_packets = 0;

void signal_handler(int sig) {
    running = 0;
}

void print_statistics(struct timeval *start_time, struct timeval *end_time) {
    double duration = (end_time->tv_sec - start_time->tv_sec) +
                     (end_time->tv_usec - start_time->tv_usec) / 1000000.0;

    double throughput_mbps = (total_bytes * 8.0) / (duration * 1000000.0);

    printf("测试结果:\n");
    printf("  发送总字节数: %llu 字节\n", total_bytes);
    printf("  发送总包数: %llu 包\n", total_packets);
    printf("  测试持续时间: %.2f 秒\n", duration);
    printf("  吞吐量: %.2f Mbps\n", throughput_mbps);
}

int main(int argc, char *argv[]) {
    int client_fd;
    struct sockaddr_in server_addr;
    char *server_ip = "127.0.0.1";
    int port = 8888;
    int packet_size = 1400;
    int duration = 10;  // Test duration in seconds
    int rate_limit = 0; // Packets per second (0 = no limit)
    int opt;
    struct timeval start_time, end_time, now;
    char *packet_buffer;
    uint32_t seq_num = 0;

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "s:p:b:t:r:")) != -1) {
        switch (opt) {
            case 's':
                server_ip = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'b':
                packet_size = atoi(optarg);
                if (packet_size < sizeof(packet_t) || packet_size > 65507) {
                    fprintf(stderr, "包大小必须在 %lu 到 65507 字节之间\n", sizeof(packet_t));
                    exit(EXIT_FAILURE);
                }
                break;
            case 't':
                duration = atoi(optarg);
                break;
            case 'r':
                rate_limit = atoi(optarg);
                break;
            default:
                fprintf(stderr, "用法: %s [-s server_ip] [-p port] [-b packet_size] [-t duration] [-r rate_limit]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Allocate buffer for sending packets
    packet_buffer = (char *)malloc(packet_size);
    if (!packet_buffer) {
        perror("内存分配失败");
        exit(EXIT_FAILURE);
    }

    // Initialize packet buffer with random data
    memset(packet_buffer, 0, packet_size);
    packet_t *packet = (packet_t *)packet_buffer;

    // Create UDP socket
    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("套接字创建失败");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("无效的服务器地址");
        exit(EXIT_FAILURE);
    }

    // Set up signal handler for graceful termination
    signal(SIGINT, signal_handler);

    printf("UDP客户端已启动，发送到 %s:%d\n", server_ip, port);
    printf("包大小: %d 字节, 测试时长: %d 秒", packet_size, duration);
    if (rate_limit > 0) {
        printf(", 速率限制: %d 包/秒", rate_limit);
    }
    printf("\n");

    // Start timing
    gettimeofday(&start_time, NULL);
    end_time.tv_sec = start_time.tv_sec + duration;
    end_time.tv_usec = start_time.tv_usec;

    // Main send loop
    while (running) {
        gettimeofday(&now, NULL);

        // Check if test duration has elapsed
        if (now.tv_sec > end_time.tv_sec ||
            (now.tv_sec == end_time.tv_sec && now.tv_usec >= end_time.tv_usec)) {
            break;
        }

        // Prepare packet header
        packet->seq_num = htonl(seq_num++);
        packet->timestamp_sec = htonl(now.tv_sec);
        packet->timestamp_usec = htonl(now.tv_usec);

        // Send packet
        int sent_bytes = sendto(client_fd, packet_buffer, packet_size, 0,
                              (struct sockaddr *)&server_addr, sizeof(server_addr));

        if (sent_bytes > 0) {
            total_bytes += sent_bytes;
            total_packets++;
        }

        // Rate limiting
        if (rate_limit > 0) {
            usleep(1000000 / rate_limit);
        }
    }

    gettimeofday(&now, NULL);
    print_statistics(&start_time, &now);

    free(packet_buffer);
    close(client_fd);
    return 0;
}