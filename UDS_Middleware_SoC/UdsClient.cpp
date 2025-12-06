#include "UdsClient.h"
#include <iostream>
#include <iomanip> // Để in hex đẹp hơn nếu cần debug

// --- BƯỚC 3.1: Constructor & Dependency Injection ---
// Nhận vào Interface Transport để sau này dễ dàng thay thế (Mock -> SocketCAN)
UdsClient::UdsClient(ITransport* trans, SecurityManager* sec, int timeoutMs)
    : transport(trans), security(sec), p2_timeout(timeoutMs) 
{
    if (!this->transport) {
        std::cerr << "[Error] UdsClient initialized with NULL transport!\n";
    }
}

UdsClient::~UdsClient() {
    // Không delete transport vì nó được quản lý bởi bên ngoài (main)
}

// --- BƯỚC 3.2: Hàm cốt lõi (Core Logic) ---
// Chịu trách nhiệm điều phối: Gửi -> Chờ -> Check Lỗi -> Trả kết quả
UdsResponse UdsClient::sendAndWait(const UdsMessage& req) {
    // 1. Kiểm tra an toàn
    if (!transport) {
        throw UdsException(UdsClient::ERR_PROTOCOL); // Lỗi 0xFE (tự quy định)
    }

    // 2. Gửi lệnh đi (Sử dụng ITransport đã cập nhật nhận UdsMessage)
    transport->send(req);

    // 3. Chờ phản hồi
    // Tạo buffer rỗng để nhận dữ liệu
    std::vector<uint8_t> rxBuffer;
    
    // Gọi hàm receive của Transport (trả về bool: true=có data, false=timeout)
    bool success = transport->receive(rxBuffer, p2_timeout);

    // 4. Xử lý Timeout
    if (!success) {
        throw UdsException(UdsClient::ERR_TIMEOUT); // Lỗi 0xFF
    }

    // 5. Phân tích gói tin nhận được
    // Tận dụng class UdsResponse "thông minh" của bạn:
    // Nó sẽ tự động tách SID, tự kiểm tra xem có phải 0x7F hay không.
    UdsResponse response(rxBuffer);

    // 6. Kiểm tra Logic UDS (Positive vs Negative)
    if (!response.isPositive()) {
        // Nếu là Negative Response (0x7F...), lấy mã NRC ra và ném Exception.
        // UdsException sẽ tự tra từ điển để in ra dòng thông báo lỗi (VD: "Invalid Key")
        throw UdsException(response.getNRC());
    }

    // 7. Nếu thành công, trả về đối tượng Response cho Application xử lý tiếp
    return response;
}

// --- BƯỚC 3.3: Các dịch vụ cơ bản (API cho Application) ---

// Service 0x10: Diagnostic Session Control
UdsResponse UdsClient::requestSession(uint8_t sessionType) {
    std::cout << "\n[Client] -> Calling API: requestSession(0x" 
              << std::hex << (int)sessionType << ")" << std::endl;

    // Tạo payload chứa tham số (Sub-function)
    std::vector<uint8_t> payload = {sessionType};
    
    // Đóng gói thành UdsMessage (SID 0x10)
    // Sử dụng Constructor 2 tham số mà ta đã thêm vào UdsMessage
    UdsMessage req(0x10, payload); 

    // Gọi hàm trung tâm để xử lý
    return sendAndWait(req);
}

// Service 0x11: ECU Reset
UdsResponse UdsClient::requestHardReset() {
    std::cout << "\n[Client] -> Calling API: requestHardReset()" << std::endl;

    // Sub-function 0x01 = Hard Reset
    std::vector<uint8_t> payload = {0x01};
    
    // Đóng gói thành UdsMessage (SID 0x11)
    UdsMessage req(0x11, payload); 

    return sendAndWait(req);
}
