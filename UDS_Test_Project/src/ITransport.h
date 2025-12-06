#ifndef ITRANSPORT_H
#define ITRANSPORT_H

#include <vector>
#include <cstdint> // Để sử dụng kiểu uint8_t (byte)
#include "UdsMessage.h"
/**
 * Interface ITransport
 * Nhiệm vụ: Định nghĩa lớp trừu tượng cho việc gửi và nhận dữ liệu.
 * Các lớp con (MockTransport, SocketCANTransport) bắt buộc phải cài đặt logic cho 2 hàm này.
 */
class ITransport {
public:
    // Destructor ảo: Cực kỳ quan trọng để tránh rò rỉ bộ nhớ khi delete đối tượng con qua con trỏ cha
    virtual ~ITransport() {}

    /**
     * Hàm gửi dữ liệu (Pure Virtual)
     * @param data: Chuỗi byte cần gửi (ví dụ: {0x10, 0x03})
     * @return: true nếu gửi thành công, false nếu thất bại
     */
    virtual void send(const UdsMessage& msg) = 0;

    /**
     * Hàm nhận dữ liệu (Pure Virtual)
     * @return: Chuỗi byte nhận được từ ECU (ví dụ: {0x50, 0x03, ...})
     * Nếu lỗi hoặc timeout, có thể trả về vector rỗng.
     */
    virtual bool receive(std::vector<uint8_t>& buffer, int timeoutMs) = 0;
};

#endif // ITRANSPORT_H
