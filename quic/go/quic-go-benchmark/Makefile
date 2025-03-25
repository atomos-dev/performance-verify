.PHONY: all build clean

all: build

build:
	@echo "构建QUIC吞吐量测试工具..."
	go build -o bin/quic-server cmd/server/main.go
	go build -o bin/quic-client cmd/client/main.go
	@echo "构建完成!"

clean:
	@echo "清理构建文件..."
	rm -rf bin/
	@echo "清理完成!"