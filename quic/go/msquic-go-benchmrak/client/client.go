package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"time"

	"github.com/noboruma/go-msquic/pkg/quic"
)

func main() {
	// 解析命令行参数
	serverAddr := flag.String("server", "localhost", "服务器地址")
	serverPort := flag.Int("port", 8888, "服务器端口")
	packetSize := flag.Int("size", 1400, "数据包大小(字节)")
	duration := flag.Int("time", 10, "测试持续时间(秒)")
	flag.Parse()

	// 创建可以被中断信号取消的上下文
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt)
	defer cancel()

	// 配置客户端
	config := quic.Config{
		MaxIncomingStreams: 1000,
		MaxIdleTimeout:     60 * time.Second,
		KeepAlivePeriod:    5 * time.Second,
		Alpn:               "go-msquic-test",
	}

	// 连接到服务器
	serverAddrWithPort := fmt.Sprintf("%s:%d", *serverAddr, *serverPort)
	fmt.Printf("连接到服务器 %s...\n", serverAddrWithPort)

	conn, err := quic.DialAddr(ctx, serverAddrWithPort, config)
	if err != nil {
		fmt.Printf("连接失败: %v\n", err)
		return
	}
	defer conn.Close()

	fmt.Println("已连接到服务器")

	// 打开流
	stream, err := conn.OpenStream()
	if err != nil {
		fmt.Printf("打开流失败: %v\n", err)
		return
	}
	defer stream.Close()

	// 准备测试数据包
	dataPacket := make([]byte, *packetSize)
	for i := range dataPacket {
		dataPacket[i] = byte(i % 256) // 填充一些模式数据
	}

	// 创建定时器控制测试持续时间
	timer := time.NewTimer(time.Duration(*duration) * time.Second)
	defer timer.Stop()

	// 记录开始时间
	startTime := time.Now()

	// 统计变量
	var totalBytesSent int64
	var packetsSent int64

	// 发送循环
	fmt.Printf("开始吞吐量测试，持续 %d 秒...\n", *duration)
sendLoop:
	for {
		select {
		case <-timer.C:
			// 测试时间到
			break sendLoop
		case <-ctx.Done():
			// 收到中断信号
			fmt.Println("\n测试被中断")
			break sendLoop
		default:
			// 发送数据包
			n, err := stream.Write(dataPacket)
			if err != nil {
				fmt.Printf("发送数据失败: %v\n", err)
				// 短暂暂停后继续
				time.Sleep(10 * time.Millisecond)
				continue
			}
			totalBytesSent += int64(n)
			packetsSent++
		}
	}

	// 计算测试结果
	elapsedTime := time.Since(startTime).Seconds()
	throughputMbps := float64(totalBytesSent*8) / (elapsedTime * 1000000)

	// 打印测试结果
	fmt.Println("\n测试结果:")
	fmt.Printf("总发送数据: %.2f MB\n", float64(totalBytesSent)/1000000)
	fmt.Printf("发送的数据包: %d\n", packetsSent)
	fmt.Printf("测试时间: %.2f 秒\n", elapsedTime)
	fmt.Printf("吞吐量: %.2f Mbps\n", throughputMbps)
}
