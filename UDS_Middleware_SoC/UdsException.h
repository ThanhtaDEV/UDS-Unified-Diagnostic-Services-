// UdsException.h
#ifndef UDS_EXCEPTION_H
#define UDS_EXCEPTION_H

#include <exception>
#include <string>
#include <cstdint>

class UdsException : public std::exception {
private:
    uint8_t nrc;        // Mã lỗi gốc (ví dụ 0x7F, 0x33...)
    std::string message; // Chuỗi thông báo lỗi (đã map từ nrc)

public:
    // Constructor: Nhận mã NRC và tự động tạo message
    explicit UdsException(uint8_t nrcCode);

    // Override hàm what() của std::exception để trả về thông báo lỗi
    const char* what() const noexcept override;

    // Getter để lấy mã NRC gốc nếu cần xử lý logic riêng
    uint8_t getNRC() const;

    // Hàm tĩnh giúp chuyển đổi Hex sang Text (Helper)
    static std::string getNrcDescription(uint8_t nrcCode);
};

#endif // UDS_EXCEPTION_H
