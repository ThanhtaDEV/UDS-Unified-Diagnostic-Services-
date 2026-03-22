#include "SocketCanTransport.h"
#include <iostream>
#include <cstring>
#include <unistd.h>		// close, read, write
#include <sys/socket.h>		// socket, bind, socketopt
#include <sys/ioctl.h>		// ioctl
#include <net/if.h>     	// ifreq
#include <linux/can.h>
#include <linux/can/isotp.h>	// Giao thức ISO-TP
#include <sys/time.h>    	// timeval
#include <iomanip>

SocketCanTransport::SocketCanTransport(const std::string& interfaceName, uint32_t transmitId, uint32_t receiveId)
    : ifName(interfaceName), txId(transmitId), rxId(receiveId), socketFd(-1) 
{
    init();
}

SocketCanTransport::~SocketCanTransport() {
    closeSocket();
}

void SocketCanTransport::closeSocket() {
    if (socketFd >= 0) {
        ::close(socketFd); // Gọi hàm close của hệ thống
        socketFd = -1;
        std::cout << "[Transport] Socket closed." << std::endl;
    }
}

bool SocketCanTransport::init() {
    // Nếu socket đang mở thì đóng trước đã
    if (socketFd >= 0) {
        closeSocket();
    }

    // 1. Tạo Socket với giao thức ISO-TP (CAN_ISOTP)
    socketFd = socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP);
    if (socketFd < 0) {
        perror("[Transport] Socket creation failed");
        return false;
    }

    // 2. Tìm interface index từ tên (ví dụ "can0" -> index)
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, ifName.c_str(), IFNAMSIZ - 1);
    
    if (ioctl(socketFd, SIOCGIFINDEX, &ifr) < 0) {
        perror("[Transport] Interface lookup failed (Check if interface exists/up)");
        closeSocket();
        return false;
    }

    // 3. Cấu hình địa chỉ ISO-TP (Source & Destination)
    struct sockaddr_can addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    addr.can_addr.tp.tx_id = txId;  // Gửi ID này
    addr.can_addr.tp.rx_id = rxId;  // Nghe ID kia

    // 4. Bind socket vào interface
    if (bind(socketFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[Transport] Bind failed");
        closeSocket();
        return false;
    }

    std::cout << "[Transport] ISO-TP Socket connected to " << ifName 
              << " (Tx: 0x" << std::hex << txId << ", Rx: 0x" << rxId << ")" << std::endl;
    return true;
}

bool SocketCanTransport::send(const UdsMessage& msg) {
    if (socketFd < 0) return false;

    // SID + Payload
    std::vector<uint8_t> data = msg.getRawBytes();

    // LOGGING: In ra data ứng dụng (Application Data)
    // (Ta không thấy frame CAN nhỏ ở đây vì Kernel đã làm ngầm)
    std::cout << "[ISO-TP Tx] Len: " << std::dec << data.size() << " | Data: [ ";
    for (const auto& byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
    }
    std::cout << "]" << std::dec << std::endl;

    // GỬI: Write thẳng buffer xuống socket
    // Kernel sẽ tự động cắt nhỏ (Segmentation) nếu dài hơn 8 byte
    ssize_t nbytes = write(socketFd, data.data(), data.size());

    if (nbytes != (ssize_t)data.size()) {
        perror("[Transport] Write error");
        return false;
    }

    return true;
}

bool SocketCanTransport::receive(std::vector<uint8_t>& buffer, int timeoutMs) {
    if (socketFd < 0) return false;

    // 1. Cấu hình Timeout cho Socket
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    if (setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
        perror("[Transport] Setsockopt failed");
        return false;
    }

    // Chuẩn bị buffer tạm (ISO-TP max payload ~4095 bytes)
    uint8_t tempBuf[4096];

    // Đọc dữ liệu
    // Hàm này sẽ BLOCK cho đến khi:
    // 1. Nhận đủ 1 gói ISO-TP hoàn chỉnh (đã được Kernel ghép nối)
    // 2. Hoặc Timeout
    ssize_t nbytes = read(socketFd, tempBuf, sizeof(tempBuf));

    if (nbytes < 0) {
        // Timeout hoặc lỗi (thường là timeout, không cần in lỗi rầm rộ)
        return false;
    }

    // LOGGING
    std::cout << "[ISO-TP Rx] Len: " << std::dec << nbytes << " | Data: [ ";
    for (int i = 0; i < nbytes; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)tempBuf[i] << " ";
    }
    std::cout << "]" << std::dec << std::endl;

    // D. Copy dữ liệu ra buffer kết quả
    buffer.assign(tempBuf, tempBuf + nbytes);
    return true;
}
