package main

import (
	"flag"
	"log"
	"quic-throughput-tester/internal/client"
	"quic-throughput-tester/internal/common"
	"time"
)

func main() {
	// 解析命令行参数
	serverAddr := flag.String("server", "localhost", "服务器地址")
	serverPort := flag.Int("port", 8888, "服务器端口")
	packetSize := flag.Int("size", 1400, "数据包大小(字节)")
	duration := flag.Int("time", 10, "测试持续时间(秒)")
	rateLimit := flag.Int("rate", 0, "发送速率限制(Mbps), 0表示无限制")
	flag.Parse()

	// 创建测试配置
	config := common.TestConfig{
		PacketSize:    *packetSize,
		Duration:      time.Duration(*duration) * time.Second,
		RateLimit:     *rateLimit,
		ServerAddress: *serverAddr,
		ServerPort:    *serverPort,
	}

	// 创建客户端实例
	cli := client.NewClient(config)

	// 执行测试
	log.Printf("开始QUIC吞吐量测试，服务器: %s:%d, 包大小: %d字节, 持续时间: %d秒",
		config.ServerAddress, config.ServerPort, config.PacketSize, *duration)

	if err := cli.Run(); err != nil {
		log.Fatalf("测试失败: %v", err)
	}
}
