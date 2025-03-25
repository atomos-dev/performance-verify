package server

import (
	"context"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"encoding/binary"
	"encoding/pem"
	"fmt"
	"io"
	"log"
	"math/big"
	"quic-throughput-tester/internal/common"
	"sync"
	"time"

	"github.com/quic-go/quic-go"
)

// Server 表示QUIC吞吐量测试服务器
type Server struct {
	port       int
	listener   *quic.Listener
	result     common.TestResult
	resultLock sync.Mutex
	startTime  time.Time
	endTime    time.Time
}

// NewServer 创建一个新的服务器实例
func NewServer(port int) *Server {
	return &Server{
		port:   port,
		result: common.TestResult{},
	}
}

// generateTLSConfig 生成测试用TLS配置
func generateTLSConfig() *tls.Config {
	key, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		panic(err)
	}

	template := x509.Certificate{
		SerialNumber: big.NewInt(1),
		NotBefore:    time.Now(),
		NotAfter:     time.Now().Add(time.Hour * 24 * 180),
		KeyUsage:     x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		ExtKeyUsage:  []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
	}

	certDER, err := x509.CreateCertificate(rand.Reader, &template, &template, &key.PublicKey, key)
	if err != nil {
		panic(err)
	}

	keyPEM := pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(key)})
	certPEM := pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: certDER})

	tlsCert, err := tls.X509KeyPair(certPEM, keyPEM)
	if err != nil {
		panic(err)
	}

	return &tls.Config{
		Certificates: []tls.Certificate{tlsCert},
		NextProtos:   []string{"quic-throughput-test"},
	}
}

// Start 启动服务器
func (s *Server) Start() error {
	addr := fmt.Sprintf("0.0.0.0:%d", s.port)
	listener, err := quic.ListenAddr(addr, generateTLSConfig(), nil)
	if err != nil {
		return fmt.Errorf("无法启动QUIC监听器: %v", err)
	}

	s.listener = listener
	log.Printf("服务器已启动，监听端口 %d", s.port)

	for {
		conn, err := listener.Accept(context.Background())
		if err != nil {
			log.Printf("接受连接错误: %v", err)
			continue
		}

		log.Printf("接受来自 %s 的新连接", conn.RemoteAddr())
		go s.handleConnection(conn)
	}
}

// handleConnection 处理单个QUIC连接
func (s *Server) handleConnection(conn quic.Connection) {
	// 重置结果统计
	s.resultLock.Lock()
	s.result = common.TestResult{}
	s.startTime = time.Now()
	s.resultLock.Unlock()

	stream, err := conn.AcceptStream(context.Background())
	if err != nil {
		log.Printf("接受流错误: %v", err)
		return
	}
	defer stream.Close()

	var expectedSeq int64 = 0
	var lastSeq int64 = -1

	headerBuf := make([]byte, 16) // 8字节序列号 + 8字节时间戳

	for {
		// 读取包头
		_, err := io.ReadFull(stream, headerBuf)
		if err != nil {
			if err != io.EOF {
				log.Printf("读取流错误: %v", err)
			}
			break
		}

		seqNum := int64(binary.BigEndian.Uint64(headerBuf[:8]))
		// timestamp := int64(binary.BigEndian.Uint64(headerBuf[8:16]))

		// 检测丢包
		if lastSeq != -1 {
			if seqNum != expectedSeq {
				missed := seqNum - expectedSeq
				s.resultLock.Lock()
				s.result.PacketsLost += missed
				s.resultLock.Unlock()
				log.Printf("检测到丢包: 预期 %d, 收到 %d, 丢失 %d 个包", expectedSeq, seqNum, missed)
			}
		}

		lastSeq = seqNum
		expectedSeq = seqNum + 1

		// 读取数据负载
		var payloadSize uint32
		if err := binary.Read(stream, binary.BigEndian, &payloadSize); err != nil {
			log.Printf("读取负载大小错误: %v", err)
			break
		}

		payload := make([]byte, payloadSize)
		_, err = io.ReadFull(stream, payload)
		if err != nil {
			log.Printf("读取负载错误: %v", err)
			break
		}

		// 更新统计信息
		s.resultLock.Lock()
		s.result.BytesReceived += int64(len(headerBuf) + 4 + len(payload))
		s.result.PacketsReceived++
		s.resultLock.Unlock()
	}

	// 测试结束，计算结果
	s.resultLock.Lock()
	s.endTime = time.Now()
	s.result.Duration = s.endTime.Sub(s.startTime)

	// 计算丢包率
	if s.result.PacketsReceived > 0 || s.result.PacketsLost > 0 {
		totalExpected := s.result.PacketsReceived + s.result.PacketsLost
		s.result.LossRate = float64(s.result.PacketsLost) / float64(totalExpected) * 100
	}

	// 计算吞吐量 (Mbps)
	s.result.Throughput = float64(s.result.BytesReceived) * 8 / 1000000 / s.result.Duration.Seconds()

	s.PrintResults()
	s.resultLock.Unlock()
}

// PrintResults 打印测试结果
func (s *Server) PrintResults() {
	fmt.Println("\n========== 服务器测试结果 ==========")
	fmt.Printf("接收总字节数: %d 字节\n", s.result.BytesReceived)
	fmt.Printf("接收总包数: %d 包\n", s.result.PacketsReceived)
	fmt.Printf("丢包数: %d 包\n", s.result.PacketsLost)
	fmt.Printf("丢包率: %.2f%%\n", s.result.LossRate)
	fmt.Printf("测试持续时间: %.2f 秒\n", s.result.Duration.Seconds())
	fmt.Printf("吞吐量: %.2f Mbps\n", s.result.Throughput)
	fmt.Println("=====================================")
}

// Stop 停止服务器
func (s *Server) Stop() {
	if s.listener != nil {
		s.listener.Close()
	}
}
