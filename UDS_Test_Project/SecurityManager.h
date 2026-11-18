#ifndef SECURITYMANAGER_H
#define SECURITYMANAGER_H

#include <cstdint> // Thư viện chứa kiểu uint32_t (4 bytes chuẩn)

class SecurityManager {
public:
    virtual ~SecurityManager() = default;

    /**
     * @brief Tính toán Key dựa trên Seed nhận được từ ECU.
     * * @param seed  : Chuỗi ngẫu nhiên 4 bytes từ ECU.
     * @param level : Cấp độ bảo mật (0x01, 0x03, ...).
     * @return uint32_t : Key (4 bytes) để gửi ngược lại cho ECU.
     */
    virtual uint32_t computeKey(uint32_t seed, int level) = 0;
};

#endif // SECURITYMANAGER_H
