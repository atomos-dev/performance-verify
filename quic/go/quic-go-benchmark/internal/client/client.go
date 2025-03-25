package client

import (
	"context"
	"crypto/tls"
	"encoding/binary"
	"fmt"
	"quic-throughput-tester/internal/common"
	"sync"
	"time"

	"github.com/quic-go/quic-go"
)

// Client 表示QUIC吞吐量测试客户端
type Client struct {
	config     common.TestConfig
	result     common.TestResult
	resultLock sync.Mutex
	startTime  time.Time
	endTime    time.Time
}

// NewClient 创建一个新的客户端实例
func NewClient(config common.TestConfig) *Client {
	return &Client{
		config: config,
		result: common.TestResult{},
	}
}

// Run 执行吞吐量测试
func (c *Client) Run() error {
	tlsConfig := &tls.Config{
		InsecureSkipVerify: true,
		NextProtos:         []string{"quic-throughput-test"},
	}

	serverAddr := fmt.Sprintf("%s:%d", c.config.ServerAddress, c.config.ServerPort)
	conn, err := quic.DialAddr(context.Background(), serverAddr, tlsConfig, nil)
	if err != nil {
		return fmt.Errorf("无法连接到服务器 %s: %v", serverAddr, err)
	}
	defer conn.CloseWithError(0, "测试完成")

	stream, err := conn.OpenStreamSync(context.Background())
	if err != nil {
		return fmt.Errorf("无法打开QUIC流: %v", err)
	}
	defer stream.Close()

	// 准备测试数据
	payload := make([]byte, c.config.PacketSize)
	for i := range payload {
		payload[i] = byte(i % 256)
	}

	// 计算速率限制
	var packetInterval time.Duration
	var ticker *time.Ticker

	if c.config.RateLimit > 0 {
		// 计算每个包的发送间隔，以达到指定的速率限制
		packetBits := (c.config.PacketSize + 20) * 8 // 包大小 + 头部大小 (估计)
		packetsPerSecond := (c.config.RateLimit * 1000000) / packetBits
		packetInterval = time.Second / time.Duration(packetsPerSecond)
		ticker = time.NewTicker(packetInterval)
		defer ticker.Stop()
	}

	// 开始测试
	c.startTime = time.Now()
	testEndTime := c.startTime.Add(c.config.Duration)

	var seqNum int64 = 0

	for time.Now().Before(testEndTime) {
		// 构建包头 (序列号 + 时间戳)
		header := make([]byte, 16)
		binary.BigEndian.PutUint64(header[:8], uint64(seqNum))
		binary.BigEndian.PutUint64(header[8:16], uint64(time.Now().UnixNano()))

		// 写入包头
		_, err := stream.Write(header)
		if err != nil {
			return fmt.Errorf("写入包头错误: %v", err)
		}

		// 写入负载大小
		err = binary.Write(stream, binary.BigEndian, uint32(len(payload)))
		if err != nil {
			return fmt.Errorf("写入负载大小错误: %v", err)
		}

		// 写入负载
		_, err = stream.Write(payload)
		if err != nil {
			return fmt.Errorf("写入负载错误: %v", err)
		}

		// 更新统计信息
		c.resultLock.Lock()
		c.result.BytesSent += int64(len(header) + 4 + len(payload))
		c.result.PacketsSent++
		c.resultLock.Unlock()

		seqNum++

		// 如果设置了速率限制，则等待下一个发送时间点
		if c.config.RateLimit > 0 {
			<-ticker.C
		}
	}

	// 测试结束
	c.endTime = time.Now()
	c.result.Duration = c.endTime.Sub(c.startTime)

	// 计算吞吐量 (Mbps)
	c.result.Throughput = float64(c.result.BytesSent) * 8 / 1000000 / c.result.Duration.Seconds()

	c.PrintResults()

	// 等待一段时间，确保服务器有时间处理所有数据
	time.Sleep(time.Second)

	return nil
}

// PrintResults 打印测试结果
func (c *Client) PrintResults() {
	fmt.Println("\n========== 客户端测试结果 ==========")
	fmt.Printf("发送总字节数: %d 字节\n", c.result.BytesSent)
	fmt.Printf("发送总包数: %d 包\n", c.result.PacketsSent)
	fmt.Printf("数据包大小: %d 字节\n", c.config.PacketSize)
	fmt.Printf("测试持续时间: %.2f 秒\n", c.result.Duration.Seconds())
	fmt.Printf("吞吐量: %.2f Mbps\n", c.result.Throughput)
	if c.config.RateLimit > 0 {
		fmt.Printf("速率限制: %d Mbps\n", c.config.RateLimit)
	}
	fmt.Println("=====================================")
}
