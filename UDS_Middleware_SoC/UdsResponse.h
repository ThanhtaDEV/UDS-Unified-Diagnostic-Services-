// UdsResponse.h
#ifndef UDS_RESPONSE_H
#define UDS_RESPONSE_H

#include <vector>
#include <cstdint>

class UdsResponse {
private:
    uint8_t sid;                // Service ID (Ví dụ: 0x50, hoặc SID gốc trong gói lỗi)
    uint8_t nrc;                // Negative Response Code (0x00 = Success)
    std::vector<uint8_t> data;  // Dữ liệu payload (đã lọc bỏ SID header)
    bool _isPositive;           // Cờ đánh dấu nhanh trạng thái
    //std::vector<uint8_t> payload;

public:
    // Constructor: Nhận raw bytes từ Transport và tự động parse
    explicit UdsResponse(const std::vector<uint8_t>& rawData);
    // UdsResponse(uint8_t id, const std::vector<uint8_t>& data);
    // Kiểm tra nhanh xem có OK không
    bool isPositive() const;

    // Getter lấy các thành phần
    uint8_t getNRC() const;
    uint8_t getSid() const;
    std::vector<uint8_t> getData() const;
};

#endif // UDS_RESPONSE_H
