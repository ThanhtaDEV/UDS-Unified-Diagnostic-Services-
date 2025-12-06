#ifndef MOCKTRANSPORT_H
#define MOCKTRANSPORT_H

#include "ITransport.h"
#include "UdsMessage.h"
#include <iostream>
#include <vector>
#include <iomanip> // Để in hex đẹp hơn

class MockTransport : public ITransport {
private:
    // Biến lưu trữ "câu trả lời giả" để dành cho hàm receive lấy ra
    std::vector<uint8_t> bufferedResponse;

public:
    // --- Hàm Gửi (Giả lập việc ECU nhận tín hiệu và chuẩn bị câu trả lời) ---
    void send(const UdsMessage& msg) override {
        uint8_t sid = msg.getSid();
        const std::vector<uint8_t>& payload = msg.getPayload(); // Sửa lại cách lấy payload cho đúng cú pháp

        // 1. In ra màn hình để ta thấy Client đang gửi gì
        std::cout << "[MockTransport] >> SENDING to ECU: " 
                  << std::hex << std::uppercase << (int)sid << " ";
        for (auto b : payload) {
            std::cout << (int)b << " ";
        }
        std::cout << std::dec << std::endl; // Xuống dòng, reset về decimal

        // 2. Logic "Script" (Kịch bản trả lời tự động)
        bufferedResponse.clear();

        if (sid == 0x10) { 
            // Nếu nhận 10 (Session) -> Trả về 50 (OK)
            // Giả sử request 10 03 -> Response 50 03 00 32 01 F4
            bufferedResponse = {0x50, payload.empty() ? (uint8_t)0x00 : payload[0], 0x00, 0x32, 0x01, 0xF4};
        }
        else if (sid == 0x11) { 
            // Nếu nhận 11 (Reset) -> Trả về 51 (OK)
            // Cấu trúc phản hồi Reset: [51] [Sub-function]
            uint8_t sf = payload.empty() ? 0x00 : payload[0];
            bufferedResponse = {0x51, sf}; 
        }
        else if (sid == 0x27) {
            // Nếu nhận 27 (Security) -> Giả bộ báo lỗi 7F (Negative Response)
            bufferedResponse = {0x7F, 0x27, 0x35}; // 0x35 = Invalid Key
        }
        else {
            // Các trường hợp khác -> Giả bộ không trả lời
        }
    }

    // --- Hàm Nhận (Client gọi hàm này để lấy kết quả) ---
    bool receive(std::vector<uint8_t>& buffer, int timeoutMs) override {
        // Giả lập timeout: Nếu không có gì trong buffer thì báo lỗi
        if (bufferedResponse.empty()) {
            return false; 
        }

        // Đổ dữ liệu từ kho "bufferedResponse" sang cho Client
        buffer = bufferedResponse;

        // In ra màn hình để debug
        std::cout << "[MockTransport] << RECEIVED from ECU: ";
        for (auto b : buffer) {
            std::cout << std::hex << std::uppercase << (int)b << " ";
        }
        std::cout << std::dec << std::endl;

        // Xóa buffer sau khi đã đọc để tránh đọc lại lần sau (giả lập giống socket thật)
        bufferedResponse.clear(); 
        
        return true; // Đã nhận thành công
    }
};

#endif // MOCKTRANSPORT_H
