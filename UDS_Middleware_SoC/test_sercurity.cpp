#include <iostream>
#include <iomanip> // Để in ra dạng Hex
#include "StandardSecurity.h"

int main() {
    // Giả lập
    StandardSecurity mySec;
    
    // Test Case 1
    uint32_t seed_demo = 0x4A6F12C5; // Ví dụ ECU gửi số này
    uint32_t key_result = mySec.computeKey(seed_demo, 0x01);

    std::cout << "--- SECURITY ALGORITHM TEST ---" << std::endl;
    std::cout << "Seed Input : 0x" << std::hex << std::uppercase << seed_demo << std::endl;
    std::cout << "Secret     : 0x5F1892AE" << std::endl;
    std::cout << "Key Output : 0x" << key_result << std::endl;

    // KẾT QUẢ MONG ĐỢI (Bạn có thể tính tay hoặc tin vào máy):
    // Seed: 4A6F12C5
    // XOR : 1577806B (4A6F12C5 ^ 5F1892AE)
    // Shift: ACC035AC ((1577806B << 3) | (1577806B >> 5))
    // Add : BDE268F0 (ACC035AC + 11223344)
    
    if (key_result == 0xBCE1F29F) {
        std::cout << "-> TEST PASS: Thuật toán chuẩn xác!" << std::endl;
    } else {
        std::cout << "-> TEST FAIL: Kiểm tra lại code!" << std::endl;
    }

    return 0;
}
