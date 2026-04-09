// UdsMessage.cpp
#include "UdsMessage.h"

// Constructor
UdsMessage::UdsMessage(uint8_t serviceId) : sid(serviceId) {
    // Payload ban đầu rỗng, vector tự động quản lý bộ nhớ
}

UdsMessage::UdsMessage(uint8_t serviceId, const std::vector<uint8_t>& data)
    : sid(serviceId), payload(data) {
}

// Thêm 1 byte
void UdsMessage::appendByte(uint8_t b) {
    payload.push_back(b);
}

// Thêm nhiều byte
void UdsMessage::appendBytes(const std::vector<uint8_t>& bytes) {
    // Chèn toàn bộ bytes vào cuối vector payload
    payload.insert(payload.end(), bytes.begin(), bytes.end());
}

// Quan trọng: Đóng gói SID và Payload thành một dòng dữ liệu liền mạch
std::vector<uint8_t> UdsMessage::getRawBytes() const {
    std::vector<uint8_t> raw;

    // Tối ưu: Dự trù trước bộ nhớ để tránh cấp phát nhiều lần
    // Kích thước = 1 byte SID + kích thước payload hiện tại
    raw.reserve(1 + payload.size());

    // 1. Đưa SID vào đầu tiên
    raw.push_back(sid);

    // 2. Đưa toàn bộ payload vào sau
    raw.insert(raw.end(), payload.begin(), payload.end());

    return raw;
}

uint8_t UdsMessage::getSid() const {
    return sid;
}

const std::vector<uint8_t>& UdsMessage::getPayload() const {
    return payload;
}

size_t UdsMessage::getPayloadSize() const {
    return payload.size();
}

// Trả về tham chiếu để bên ngoài có thể push_back vào vector này
std::vector<uint8_t>& UdsMessage::getPayloadVector() {
    return payload;
}
