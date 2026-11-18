#include <iostream>
#include "MockTransport.h"
#include "UdsClient.h"
#include "UdsException.h"

int main() {
    // 1. Tạo Mock Transport (Cơ thể giả)
    MockTransport mockInfo;

    // 2. Tạo Client (Bộ não), tiêm Mock vào
    UdsClient client(&mockInfo);

    std::cout << "=== STEP 3.3: TEST SERVICE APIS ===\n" << std::endl;

    // --- TEST 1: Request Session (Thành công) ---
    try {
        UdsResponse resp = client.requestSession(0x03);

        // SỬA: Dùng getData() thay vì getPayload()
        if (!resp.getData().empty()) {
            std::cout << "-> OK. Current Session: 0x"
                      << std::hex << (int)resp.getData()[0] << std::endl;
        } else {
            std::cout << "-> OK (No Data)." << std::endl;
        }

    } catch (const UdsException& e) {
        // SỬA: Dùng getNRC() (viết hoa) thay vì getNrc()
        // Dùng e.what() để in ra thông báo lỗi tiếng Anh đầy đủ
        std::cout << "-> FAILED. " << e.what() 
                  << " (NRC: 0x" << std::hex << (int)e.getNRC() << ")" << std::endl;
    }

    // --- TEST 2: Hard Reset ---
    try {
        client.requestHardReset();
        std::cout << "-> Reset OK." << std::endl;
    } catch (const UdsException& e) {
        // SỬA: Dùng getNRC() và what()
        std::cout << "-> Reset Error. " << e.what() 
                  << " (NRC: 0x" << std::hex << (int)e.getNRC() << ")" << std::endl;
    }

    return 0;
}
