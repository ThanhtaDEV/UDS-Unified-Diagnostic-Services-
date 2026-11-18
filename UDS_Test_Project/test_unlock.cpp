#include <iostream>
#include <vector>
#include <cstring>
#include <iomanip>

// Include các file trong project của bạn
#include "UdsClient.h"
#include "StandardSecurity.h"
#include "ITransport.h"

// --- TẠO MỘT MOCK TRANSPORT "KHÔN" (SMART MOCK) ---
class SmartMockTransport : public ITransport {
private:
    UdsMessage lastRequest; // Lưu lại lệnh vừa gửi để hàm receive biết đường trả lời

public:
    SmartMockTransport() : lastRequest(0) {}

    bool send(const UdsMessage& msg) override {
        // Lưu lại tin nhắn để xử lý sau
        lastRequest = msg; 
        
        // Debug: In ra lệnh Client vừa gửi
        std::cout << "[MockHW] Recv from Client: SID 0x" << std::hex << (int)msg.getSid();
        const auto& pay = msg.getPayload();
        for (auto b : pay) std::cout << " " << std::setw(2) << std::setfill('0') << (int)b;
        std::cout << std::endl;
        
        return true;
    }

    bool receive(std::vector<uint8_t>& buffer, int timeoutMs) override {
        // Logic giả lập phản hồi của ECU
        buffer.clear();

        uint8_t sid = lastRequest.getSid();
        const auto& payload = lastRequest.getPayload();

        // --- TRƯỜNG HỢP 1: REQUEST SEED (27 01) ---
        if (sid == 0x27 && !payload.empty() && payload[0] == 0x01) {
            std::cout << "[MockHW] -> Sending Seed..." << std::endl;
            // Giả lập gửi về: 67 01 [4A 6F 12 C5]
            buffer = {0x67, 0x01, 0x4A, 0x6F, 0x12, 0xC5};
            return true;
        }

        // --- TRƯỜNG HỢP 2: SEND KEY (27 02 [KEY...]) ---
        if (sid == 0x27 && !payload.empty() && payload[0] == 0x02) {
            std::cout << "[MockHW] -> Verifying Key..." << std::endl;
            
            // Key đúng mong đợi (dựa trên Seed 4A6F12C5 và thuật toán StandardSecurity)
            // Key = 0xBCE1F29F -> Byte: BC E1 F2 9F
            if (payload.size() >= 5 &&
                payload[1] == 0xBC && 
                payload[2] == 0xE1 && 
                payload[3] == 0xF2 && 
                payload[4] == 0x9F) {
                
                // Key đúng -> Unlock thành công (67 02)
                buffer = {0x67, 0x02};
            } else {
                // Key sai -> Báo lỗi (7F 27 35 - Invalid Key)
                buffer = {0x7F, 0x27, 0x35};
            }
            return true;
        }

        // Mặc định trả về lỗi nếu không hiểu lệnh
        buffer = {0x7F, sid, 0x11}; // Service Not Supported
        return true;
    }
};

// --- HÀM MAIN ĐỂ TEST ---
int main() {
    std::cout << "=== TEST STEP 4.2: UNLOCK SECURITY LOGIC ===\n" << std::endl;

    // 1. Khởi tạo Mock Transport
    SmartMockTransport* mockHw = new SmartMockTransport();

    // 2. Khởi tạo Security Manager (Thuật toán tính Key)
    StandardSecurity* mySec = new StandardSecurity();

    // 3. Khởi tạo UdsClient (Dependency Injection)
    UdsClient client(mockHw, mySec);

    try {
        // 4. Gọi hàm cần test: unlockSecurity(0x01)
        bool result = client.unlockSecurity(0x01);

        std::cout << "\n----------------------------------------" << std::endl;
        if (result) {
            std::cout << ">>> TEST PASSED: ECU Unlocked successfully! <<<" << std::endl;
        } else {
            std::cout << ">>> TEST FAILED: Could not unlock ECU. <<<" << std::endl;
        }
    } 
    catch (const std::exception& e) {
        std::cout << "EXCEPTION: " << e.what() << std::endl;
    }

    // Cleanup
    delete mockHw;
    delete mySec;

    return 0;
}
