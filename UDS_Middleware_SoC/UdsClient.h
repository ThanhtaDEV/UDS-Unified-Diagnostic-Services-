#ifndef UDSCLIENT_H
#define UDSCLIENT_H

#include <iostream>
#include <vector>

// --- KẾT NỐI VỚI GIAI ĐOẠN 2 (Transport) ---
#include "ITransport.h"

// --- KẾT NỐI VỚI GIAI ĐOẠN 1 (Data Types) ---
#include "UdsMessage.h"
#include "UdsResponse.h"
#include "UdsException.h"

// Forward declaration cho Security (để dành cho sau này, đúng như sơ đồ)
class SecurityManager; 

class UdsClient {
private:
    // Dependency 1: Giao tiếp (Cái miệng/tai)
    ITransport* transport;
    
    // Dependency 2: Bảo mật (để null nếu chưa dùng)
    SecurityManager* security;

    // Cấu hình: Thời gian chờ phản hồi (P2 Client)
    int p2_timeout;

public:
    // Định nghĩa mã lỗi nội bộ (tương thích với UdsException chỉ nhận uint8_t)
    static const uint8_t ERR_TIMEOUT = 0xFF; 
    static const uint8_t ERR_PROTOCOL = 0xFE;

    /**
     * CONSTRUCTOR (Bước 3.1)
     * Nguyên lý Dependency Injection:
     * Client không tự tạo Transport, nó nhận Transport từ bên ngoài.
     */
    UdsClient(ITransport* trans, SecurityManager* sec = nullptr, int timeoutMs = 1000);

    virtual ~UdsClient();

    bool unlockSecurity(int level);

    // Hàm nội bộ gửi và nhận (sẽ làm ở Bước 3.2)
    UdsResponse sendAndWait(const UdsMessage& req); 

    // --- BƯỚC 3.3: Các dịch vụ cơ bản (APIs) ---
    // Service 0x10: Chuyển phiên làm việc
    UdsResponse requestSession(uint8_t sessionType);

    // Service 0x11: Reset ECU
    UdsResponse requestHardReset(); 
};

#endif // UDSCLIENT_H
