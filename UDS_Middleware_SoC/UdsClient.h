#ifndef UDSCLIENT_H
#define UDSCLIENT_H

#include <iostream>
#include <vector>
#include <cstdint>
// Đồng bộ luồng - Thread Safety
#include <mutex>
#include <condition_variable>

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

    // QUẢN LÝ TRANH CHẤP TÀI NGUYÊN BUS CAN
    //std::mutex m_busAccessMutex;
    //std::condition_variable m_busAvailabilityCv;
    //bool is_fota_locked;	// Cờ đánh dấu FOTA đang chiếm dụng bus

    // HELPER FUNCTION (IMPORTANT)
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

    /**
     * @brief Khóa Bus CAN để chạy FOTA. Các luồng khác gọi sendAndWait sẽ bị ru ngủ sâu.
     */
    //void lockForFota();

    /**
     * @brief Nhả Bus CAN sau khi FOTA xong. Đánh thức các luồng đang ngủ (Diag) dậy.
     */
    //void unlockFota();

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

    /**
     * @brief Service 0x19 0x01: Đếm số lượng mã lỗi hiện tại
     * @param statusMask Mặt nạ lỗi (Ví dụ: 0x09 cho Active faults)
     * @return UdsResponse Chứa payload chứa số lượng lỗi
     */
    //UdsResponse readDTCCount(uint8_t statusMask = 0x09);

    /**
     * @brief Service 0x19 0x02: Lấy danh sách chi tiết các mã lỗi
     * @param statusMask Mặt nạ lỗi (Ví dụ: 0x09 cho Active faults)
     * @return UdsResponse Chứa chuỗi các byte DTC (3 byte) và Status (1 byte)
     */
    //UdsResponse readDTCList(uint8_t statusMask = 0x09);

    /**
     * @brief Service 0x19 0x04: Đọc Snapshot (dữ liệu đóng băng) của 1 lỗi cụ thể
     * @param dtc Ký hiệu DTC 3 byte (Ví dụ: 0xD01000)
     * @param recordNumber Số thứ tự bản ghi (Mặc định 0x01)
     * @return UdsResponse Dữ liệu môi trường lúc xảy ra lỗi
     */
    //UdsResponse readDTCSnapshot(uint32_t dtc, uint8_t recordNumber = 0x01);

    /**
     * @brief Service 0x14: Xóa toàn bộ mã lỗi trên VCU
     * @param groupOfDTC Nhóm lỗi cần xóa (Mặc định 0xFFFFFF là xóa tất cả)
     * @return UdsResponse
     */
    //UdsResponse clearDiagnosticInformation(uint32_t groupOfDTC = 0xFFFFFF);

};

#endif // UDSCLIENT_H
