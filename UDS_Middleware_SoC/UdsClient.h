#ifndef UDSCLIENT_H
#define UDSCLIENT_H

#include <iostream>
#include <vector>
#include <cstdint>

// KẾT NỐI VỚI "ITransport"
#include "ITransport.h"

// KẾT NỐI VỚI "Data Types"
#include "UdsMessage.h"
#include "UdsResponse.h"
#include "UdsException.h"
#include "UdsConstants.h"

// Forward declaration cho Security (cho sau này)
class SecurityManager;

class UdsClient {
private:
    // Dependency 1: Giao tiếp
    ITransport* transport;

    // Dependency 2: Bảo mật (để null nếu chưa dùng)
    SecurityManager* security;

    // Cấu hình: Thời gian chờ phản hồi (P2 Client)
    int p2_timeout;

    // HELPER FUNCTION (QUAN TRỌNG)
    // Hàm tách số 32-bit thành 4 byte để đẩy vào vector (dùng cho Address & Size)
    void append32BitToVector(std::vector<uint8_t>& vec, uint32_t value);

public:
    // Định nghĩa mã lỗi nội bộ (tương thích với UdsException chỉ nhận uint8_t)
    static const uint8_t ERR_TIMEOUT = 0xFF;
    static const uint8_t ERR_PROTOCOL = 0xFE;

    /**
     * CONSTRUCTOR
     * Nguyên lý Dependency Injection:
     * Client không tự tạo Transport, nó nhận Transport từ bên ngoài.
     */
    UdsClient(ITransport* trans, SecurityManager* sec = nullptr, int timeoutMs = 1000);

    virtual ~UdsClient();

    // Hàm nội bộ gửi và nhận
    UdsResponse sendAndWait(const UdsMessage& req);

    // =============================
    // CÁC DỊCH VỤ CƠ BẢN (UDS APIs)
    // =============================

    // Service 0x10: Session Control
    UdsResponse requestSession(uint8_t sessionType);

    // Service 0x3E: Tester Present
    void sendTesterPresent();

    // Service 0x11: Reset ECU
    UdsResponse requestHardReset();

    // Service 0x27: Security Access
    bool unlockSecurity(uint8_t level);

    // Service 0x31: Routine Control (Dùng để Xóa bộ nhớ)
    //UdsResponse routineControl(uint16_t routineId, uint8_t subFunc, const std::vector<uint8_t>& optionRecord);

    // Service 0x34: Request Download
    /**
     * @brief Service 0x34: Request Download (Yêu cầu tải Firmware xuống ECU)
     * * Hàm sẽ đóng gói địa chỉ (Address) và kích thước (Size) thành bản tin 0x34.
     * Tự động chèn DFI = 0x00 (Raw Data) và ALFID = 0x44 (4 byte Size, 4 byte Address).
     * * @param address Địa chỉ bắt đầu ghi trên bộ nhớ Flash của VCU (VD: 0x08040000)
     * @param size Tổng kích thước của file Firmware (.bin) bao gồm cả header
     * @return uint32_t Trả về kích thước Block tối đa (MaxBlockLength) mà VCU cho phép (VD: 1024).
     * Nếu trả về 0 nghĩa là yêu cầu bị VCU từ chối (lỗi NRC hoặc Timeout).
     */
    uint32_t requestDownload(uint32_t address, uint32_t size);

    // Service 0x36: Transfer Data
    bool transferData(uint8_t blockSequenceCounter, const std::vector<uint8_t>& dataChunk);

    // Service 0x37: Request Transfer Exit (Kết thúc nạp)
    bool requestTransferExit();

};

#endif // UDSCLIENT_H
