package main

import (
	"context"
	"fmt"
	"io"
	"os"
	"os/signal"
	"time"

	"github.com/noboruma/go-msquic/pkg/quic"
)

func main() {
	// 创建可以被中断信号取消的上下文
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt)
	defer cancel()

	// 配置服务器
	config := quic.Config{
		MaxIncomingStreams: 1000,
		CertFile:           "./server.cert",
		KeyFile:            "./server.key",
		Alpn:               "go-msquic-test",
	}

	// 监听地址
	fmt.Println("启动吞吐量测试服务器，监听端口 8888...")
	listener, err := quic.ListenAddr("0.0.0.0:8888", config)
	if err != nil {
		fmt.Printf("监听失败: %v\n", err)
		return
	}
	defer listener.Close()

	fmt.Println("等待客户端连接...")

	// 统计变量
	var totalConnections int
	var totalBytesReceived int64

	// 处理连接的函数
	handleConnection := func(conn quic.Connection) {
		defer conn.Close()

		connID := totalConnections
		totalConnections++
		fmt.Printf("客户端 #%d 已连接\n", connID)

		// 接受客户端流
		stream, err := conn.AcceptStream(ctx)
		if err != nil {
			fmt.Printf("接受流失败: %v\n", err)
			return
		}
		defer stream.Close()

		// 读取缓冲区
		buffer := make([]byte, 2048) // 使用较大的缓冲区
		var connectionBytesReceived int64

		// 持续读取数据并丢弃
		for {
			n, err := stream.Read(buffer)
			if err != nil {
				if err != io.EOF {
					fmt.Printf("读取数据失败: %v\n", err)
				}
				break
			}

			// 更新统计信息
			connectionBytesReceived += int64(n)
			totalBytesReceived += int64(n)

			// 每接收 100MB 数据打印一次状态
			if connectionBytesReceived%(100*1000*1000) < int64(n) {
				fmt.Printf("客户端 #%d: 已接收 %.2f MB 数据\n",
					connID, float64(connectionBytesReceived)/1000000)
			}
		}

		fmt.Printf("客户端 #%d 断开连接，总共接收 %.2f MB 数据\n",
			connID, float64(connectionBytesReceived)/1000000)
	}

	// 接受连接循环
	go func() {
		for {
			select {
			case <-ctx.Done():
				return
			default:
				conn, err := listener.Accept(ctx)
				if err != nil {
					if ctx.Err() == nil { // 只有在非取消情况下打印错误
						fmt.Printf("接受连接失败: %v\n", err)
					}
					continue
				}
				go handleConnection(conn)
			}
		}
	}()

	// 定期打印总统计信息
	ticker := time.NewTicker(10 * time.Second)
	defer ticker.Stop()

	go func() {
		for {
			select {
			case <-ticker.C:
				fmt.Printf("总计: %d 个连接，接收 %.2f MB 数据\n",
					totalConnections, float64(totalBytesReceived)/1000000)
			case <-ctx.Done():
				return
			}
		}
	}()

	// 等待中断信号
	<-ctx.Done()
	fmt.Println("\n服务器正在关闭...")
	fmt.Printf("总计: %d 个连接，接收 %.2f MB 数据\n",
		totalConnections, float64(totalBytesReceived)/1000000)
}
