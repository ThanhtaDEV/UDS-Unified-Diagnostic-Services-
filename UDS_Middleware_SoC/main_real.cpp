#include <iostream>
#include <thread>
#include <chrono>
#include "UdsClient.h"
#include "StandardSecurity.h"
#include "SocketCanTransport.h" // Include file mới viết

int main() {
    std::cout << "=== REAL ISO-TP TEST ON VCAN0 ===\n";

    // 1. Khởi tạo Transport thật
    // Interface: "vcan0" (hoặc "can0" nếu có phần cứng)
    // Tx ID: 0x7E0 (Gửi đến ECU)
    // Rx ID: 0x7E8 (Lắng nghe từ ECU)
    SocketCanTransport* transport = new SocketCanTransport("vcan0", 0x7E0, 0x7E8);

    // 2. Khởi tạo kết nối Socket
    if (!transport->init()) {
        std::cerr << "Khong the mo Socket CAN! Kiem tra lai 'vcan0' da bat chua?\n";
        delete transport;
        return -1;
    }

    // 3. Khởi tạo Security & Client
    StandardSecurity* security = new StandardSecurity();
    UdsClient client(transport, security, 5000);

    try {
        // 4. Thử gửi lệnh Request Session (0x10 03)
        // Vì đây là môi trường thật (hoặc vcan), nếu không có ECU thật nào đang chạy
        // để trả lời 0x7E8, thì hàm này sẽ bị TIMEOUT sau 1 giây.
        std::cout << "[App] Sending Request Session...\n";
        client.requestSession(0x03); 

    } catch (const std::exception& e) {
        std::cout << "EXCEPTION: " << e.what() << std::endl;
        std::cout << "(Luu y: Timeout la binh thuong neu khong co ECU mo phong ben kia)\n";
    }

    delete security;
    delete transport;
    return 0;
}
