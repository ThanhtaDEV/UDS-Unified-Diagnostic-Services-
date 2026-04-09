#ifndef SOCKET_CAN_TRANSPORT_H
#define SOCKET_CAN_TRANSPORT_H

#include "ITransport.h"
#include <string>
#include <vector>
#include <cstdint>

class SocketCanTransport : public ITransport {
private:
    int socketFd;	// File Descriptor quản lý kết nối socket
    std::string ifName; // can0 or vcan0
    uint32_t txId;	// Client -> ECU
    uint32_t rxId;	// ECU -> Client

public:
    /**
     * @brief Constructor
     * @param interfaceName: Tên card mạng CAN (can0)
     * @param transmitId: ID gửi đi (ECU Request ID, 0x7E0)
     * @param receiveId: ID nhận về (ECU Response ID, 0x7E8)
     */
    SocketCanTransport(const std::string& interfaceName, uint32_t transmitId, uint32_t receiveId);

    ~SocketCanTransport();

    // Khởi tạo kết nối (mở socket, bind)
    bool init();

    // Kiểm tra socket có hợp lệ không
    bool isValid() const { return socketFd >= 0; }

    // Đóng kết nối
    void closeSocket();

    // Hiện thực hoá Interface Transport
    bool send(const UdsMessage& msg) override; 				 // gửi message xuống kernel
    bool receive(std::vector<uint8_t>& buffer, int timeoutMs) override;  // nhận message từ kernel
};

#endif // SOCKET_CAN_TRANSPORT_H

