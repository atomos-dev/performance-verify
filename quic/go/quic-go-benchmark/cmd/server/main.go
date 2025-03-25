package main

import (
	"flag"
	"log"
	"os"
	"os/signal"
	"quic-throughput-tester/internal/server"
	"syscall"
)

func main() {
	// 解析命令行参数
	port := flag.Int("port", 8888, "服务器监听端口")
	flag.Parse()

	// 创建服务器实例
	srv := server.NewServer(*port)

	// 处理信号以优雅退出
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		<-sigChan
		log.Println("接收到退出信号，正在关闭服务器...")
		srv.Stop()
		os.Exit(0)
	}()

	// 启动服务器
	log.Printf("启动QUIC吞吐量测试服务器，端口: %d", *port)
	if err := srv.Start(); err != nil {
		log.Fatalf("服务器启动失败: %v", err)
	}
}
