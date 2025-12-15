#include "UdsClient.h"
#include <iostream>
#include <iomanip> // Để in hex đẹp hơn nếu cần debug
#include "SecurityManager.h"

// BƯỚC 3.1: Constructor & Dependency Injection
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

// BƯỚC 3.2: Hàm cốt lõi (Core Logic)
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

    // 7. Kiểm tra SID khớp lệnh
    uint8_t expectedSid = req.getSid() + 0x40;
    if (response.getSid() != expectedSid) {
        std::cerr << "[Error] Protocol Mismatch! Req: 0x" << std::hex << (int)req.getSid()
                  << " -> Exp: 0x" << (int)expectedSid
                  << ", Got: 0x" << (int)response.getSid() << std::endl;
        
        // Ném lỗi giao thức (Protocol Error)
        throw UdsException(UdsClient::ERR_PROTOCOL);
    }

    //8. mọi thứ ok thì trả về
    return response;
}

// BƯỚC 3.3: Các dịch vụ cơ bản (API cho Application)

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

/**
 IMPLEMENTATION: unlockSecurity
 * Phù hợp với Architecture: Service Logic (The Brain)
 */
bool UdsClient::unlockSecurity(int level) {
    // 1. Kiểm tra xem đã có "Chiến lược bảo mật" (Security Strategy) chưa
    if (this->security == nullptr) {
        std::cout << "[Client] Error: No SecurityManager injected!" << std::endl;
        return false;
    }

    std::cout << "[Client] Starting Security Access (Level 0x" << std::hex << level << ")..." << std::endl;

    // BƯỚC A: REQUEST SEED (Gửi lệnh 27 01)
    UdsMessage reqSeed(0x27);
    reqSeed.appendByte(static_cast<uint8_t>(level));

    // Gọi Transport gửi đi và chờ nhận về (Orchestrator Logic)
    UdsResponse respSeed = sendAndWait(reqSeed);

    // Kiểm tra phản hồi (Validate Response)
    // Phải là Positive (0x67) và đủ độ dài (1 byte sub-func + 4 bytes Seed)
    if (respSeed.getSid() != 0x67 || respSeed.getData().size() < 5) {
        std::cout << "[Client] Failed to get Seed. Response invalid or Error (NRC)." << std::endl;
        return false; // Nếu gặp NRC (0x7F) thì sendAndWait đã xử lý phần nào, ở đây ta báo fail.
    }

    // BƯỚC B: TRÍCH XUẤT SEED (Deserialize)
    // Seed từ ECU gửi về là dạng Vector Byte [S1, S2, S3, S4]
    // Ta phải ghép lại thành số uint32_t để tính toán
    const std::vector<uint8_t>& seedData = respSeed.getData();
    uint32_t seed = 0;
    seed |= (uint32_t)seedData[1] << 24;
    seed |= (uint32_t)seedData[2] << 16;
    seed |= (uint32_t)seedData[3] << 8;
    seed |= (uint32_t)seedData[4];

    std::cout << "[Client] Got Seed: 0x" << std::hex << seed << std::endl;

    // BƯỚC C: TÍNH KEY (Strategy Pattern execution)
    // UdsClient không tự tính, mà nhờ SecurityManager tính
    uint32_t key = security->computeKey(seed, level);
    std::cout << "[Client] Computed Key: 0x" << std::hex << key << std::endl;

    // BƯỚC D: GỬI KEY (Send Key - 27 02)
    UdsMessage sendKeyMsg(0x27);
    // Quy tắc: Request lẻ (01), Send Key chẵn (02) -> Level + 1
    sendKeyMsg.appendByte(static_cast<uint8_t>(level + 1)); 

    // Tách Key (uint32_t) thành Vector Byte để gửi qua Transport
    sendKeyMsg.appendByte((key >> 24) & 0xFF);
    sendKeyMsg.appendByte((key >> 16) & 0xFF);
    sendKeyMsg.appendByte((key >> 8) & 0xFF);
    sendKeyMsg.appendByte(key & 0xFF);

    UdsResponse respKey = sendAndWait(sendKeyMsg);

    // BƯỚC E: KIỂM TRA KẾT QUẢ CUỐI CÙNG
    std::vector<uint8_t> keyData = respKey.getData();

    if (respKey.getSid() == 0x67 && !keyData.empty() && keyData[0] == (level + 1)) {
        std::cout << "[Client] Security Access UNLOCKED successfully!" << std::endl;
        return true;
    } else {
        std::cout << "[Client] Unlock Failed! Key rejected." << std::endl;
        return false;
    }
}
