// UdsResponse.cpp
#include "UdsResponse.h"

UdsResponse::UdsResponse(const std::vector<uint8_t>& rawData) {
    // 1. Kiểm tra an toàn: Dữ liệu rỗng
    if (rawData.empty()) {
        sid = 0;
        nrc = 0xFF; // Lỗi nội bộ
        _isPositive = false;
        return;
    }

    // 2. LOGIC UDS: Kiểm tra Byte đầu tiên
    // Theo chuẩn ISO 14229: Nếu byte đầu là 0x7F -> Negative Response
    if (rawData[0] == 0x7F) {
        _isPositive = false;

        // Cấu trúc gói lỗi: [0x7F] [Original SID] [NRC]
        // Cần ít nhất 3 byte để đọc được mã lỗi
        if (rawData.size() >= 3) {
            sid = rawData[1]; // SID gốc mà mình đã gửi đi
            nrc = rawData[2]; // Mã lỗi (Ví dụ: 0x11, 0x12, 0x33...)
        } else {
            sid = 0;
            nrc = 0xFF; // Gói tin lỗi bị cụt (Malformed)
        }
        // Gói lỗi không có payload data
    }
    else {
        // 3. Trường hợp Positive Response (Thành công)
        // Cấu trúc: [SID phản hồi] [Data Byte 1] [Data Byte 2]...
        _isPositive = true;
        sid = rawData[0]; // Byte đầu là SID (thường là Request SID + 0x40)
        nrc = 0x00;       // 0x00 đại diện cho Success (không có lỗi)

        // Tách Payload: Copy từ byte thứ 2 trở đi vào vector data
        if (rawData.size() > 1) {
            data.assign(rawData.begin() + 1, rawData.end());
        }
    }
}

bool UdsResponse::isPositive() const {
    return _isPositive;
}

uint8_t UdsResponse::getNRC() const {
    return nrc;
}

uint8_t UdsResponse::getSid() const {
    return sid;
}

std::vector<uint8_t> UdsResponse::getData() const {
    return data;
}
