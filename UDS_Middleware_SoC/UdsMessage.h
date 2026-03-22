// UdsMessage.h
#ifndef UDS_MESSAGE_H
#define UDS_MESSAGE_H

#include <vector>
#include <cstdint> // Để dùng uint8_t

class UdsMessage {
private:
    uint8_t sid;                     // Service ID (ví dụ: 0x10, 0x11, 0x3E...)
    std::vector<uint8_t> payload;    // Dữ liệu đi kèm (tham số)

public:
    // Constructor: Khởi tạo gói tin với SID
    explicit UdsMessage(uint8_t serviceId);

    UdsMessage(uint8_t serviceId, const std::vector<uint8_t>& data);

    // Thêm 1 byte vào payload
    void appendByte(uint8_t b);

    // Thêm một mảng byte (vector) vào payload
    void appendBytes(const std::vector<uint8_t>& bytes);

    // Lấy toàn bộ gói tin (SID + Payload) để gửi xuống lớp dưới
    std::vector<uint8_t> getRawBytes() const;

    // Getter (tiện ích nếu cần kiểm tra lại SID)
    uint8_t getSid() const;

    const std::vector<uint8_t>& getPayload() const;

    // Getter lấy độ dài payload (hữu ích để tính Data Length Code - DLC)
    size_t getPayloadSize() const;

    // Lấy Payload dạng tham chiếu (Reference) để chỉnh sửa trực tiếp
    // Hàm này cần thiết cho: append32BitToVector(msg.getPayloadVector(), val)
    std::vector<uint8_t>& getPayloadVector();
};

#endif // UDS_MESSAGE_H
