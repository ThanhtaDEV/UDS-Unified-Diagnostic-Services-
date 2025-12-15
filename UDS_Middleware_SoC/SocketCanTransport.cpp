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

SocketCanTransport::SocketCanTransport(const std::string& interfaceName, uint32_t transmitId, uint32_t receiveId)
    : ifName(interfaceName), txId(transmitId), rxId(receiveId), socketFd(-1) {}

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

    // Lấy dữ liệu thô (SID + Payload) từ message
    // Kernel Linux sẽ tự lo việc cắt nhỏ (Segmentation) thành các frame 8 byte
    std::vector<uint8_t> data = msg.getRawBytes();

    // Gửi xuống socket
    ssize_t nbytes = write(socketFd, data.data(), data.size());

    if (nbytes != (ssize_t)data.size()) {
        perror("[Transport] Write error");
        return false;
    }
    return true;
}

bool SocketCanTransport::receive(std::vector<uint8_t>& buffer, int timeoutMs) {
    if (socketFd < 0) return false;

    // Cấu hình Timeout cho Socket (System Call)
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    
    if (setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
        perror("[Transport] Setsockopt failed");
        return false;
    }

    // Chuẩn bị buffer để nhận dữ liệu
    // ISO-TP 2004 hỗ trợ tối đa 4095 bytes. Ta dùng 4096 cho chẵn.
    uint8_t tempBuf[4096]; 
    
    // Đọc từ socket (Hàm này sẽ BLOCK chờ cho đến khi nhận đủ gói hoặc Timeout)
    // Kernel Linux tự lo việc ghép nối (Reassembly) các frame nhỏ thành gói to
    ssize_t nbytes = read(socketFd, tempBuf, sizeof(tempBuf));

    if (nbytes < 0) {
        // Nếu nbytes < 0, thường là do Timeout hoặc lỗi Socket
        // (Không cần in perror nếu muốn im lặng khi timeout)
        return false; 
    }

    // Copy dữ liệu nhận được vào vector kết quả
    buffer.assign(tempBuf, tempBuf + nbytes);
    return true;
}
