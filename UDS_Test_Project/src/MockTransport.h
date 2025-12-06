#ifndef MOCKTRANSPORT_H
#define MOCKTRANSPORT_H

#include "ITransport.h"
#include "UdsMessage.h"
#include <iostream>
#include <vector>
#include <deque> // Thư viện hàng đợi (Queue)
#include <iomanip>

class MockTransport : public ITransport {
private:
    // Hàng đợi chứa các câu trả lời đã được nạp sẵn từ file text
    std::deque<std::vector<uint8_t>> responseQueue;

public:
    // --- HÀM MỚI: Nạp đạn (Pre-load response) ---
    // Test Runner sẽ gọi hàm này để nhét đáp án vào trước khi Client chạy
    void pushResponse(const std::vector<uint8_t>& res) {
        responseQueue.push_back(res);
    }

    // --- Hàm Gửi (Chỉ in ra log để kiểm tra) ---
    void send(const UdsMessage& msg) override {
        std::cout << "[Mock] >> SENDING: " << std::hex << std::uppercase;
        std::cout << (int)msg.getSid() << " ";
        for (auto b : msg.getPayload()) {
            std::cout << (int)b << " ";
        }
        std::cout << std::dec << std::endl;
    }

    // --- Hàm Nhận (Lấy đáp án từ hàng đợi ra) ---
    bool receive(std::vector<uint8_t>& buffer, int timeoutMs) override {
        // Nếu hàng đợi rỗng -> Coi như Timeout (ECU không trả lời)
        if (responseQueue.empty()) {
            std::cout << "[Mock] << TIMEOUT (Queue empty)" << std::endl;
            return false;
        }

        // Lấy câu trả lời đầu tiên trong hàng đợi
        buffer = responseQueue.front();
        responseQueue.pop_front(); // Xóa đi sau khi đã lấy

        // In log
        std::cout << "[Mock] << RECEIVED: ";
        for (auto b : buffer) {
            std::cout << std::hex << std::uppercase << (int)b << " ";
        }
        std::cout << std::dec << std::endl;

        return true;
    }
};

#endif // MOCKTRANSPORT_H
