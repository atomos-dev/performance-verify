package common

import (
	"time"
)

// TestConfig 包含测试的配置参数
type TestConfig struct {
	PacketSize    int           // 数据包大小（字节）
	Duration      time.Duration // 测试持续时间
	RateLimit     int           // 发送速率限制 (Mbps)
	ServerAddress string        // 服务器地址
	ServerPort    int           // 服务器端口
}

// TestResult 包含测试结果
type TestResult struct {
	BytesSent       int64         // 发送的总字节数
	BytesReceived   int64         // 接收的总字节数
	PacketsSent     int64         // 发送的总包数
	PacketsReceived int64         // 接收的总包数
	Duration        time.Duration // 测试持续时间
	PacketsLost     int64         // 丢失的包数
	LossRate        float64       // 丢包率
	Throughput      float64       // 吞吐量 (Mbps)
}

// DataPacket 表示测试数据包
type DataPacket struct {
	SequenceNumber int64  // 序列号
	Timestamp      int64  // 发送时间戳
	Data           []byte // 数据负载
}
