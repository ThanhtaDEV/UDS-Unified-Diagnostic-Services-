#ifndef STANDARD_SECURITY_H
#define STANDARD_SECURITY_H

#include "SecurityManager.h"

class StandardSecurity: public SecurityManager {
private:
    //Thống nhất Constant Client và Server (SoC & VCU)
    const uint32_t SECRET_CONSTANT = 0x5F1892AE;
    // Giá trị cộng thêm vào cuối
    const uint32_t SALT_VALUE = 0x11223344;

public:
    uint32_t computeKey(uint32_t seed, int level) override {
	uint32_t key = 0;
	// xử lý Level 0x01 (Programming Session Unlock)
	// Feature: Level 0x03, 0x05 thêm else if
	if (level == 0x01) {
	    // Thuật toán XOR
	    uint32_t temp = seed ^ SECRET_CONSTANT;
	    // Thuật toán Shift (dịch bit) và OR
	    temp = (temp << 3) | (temp >> 5);
	    // Thuật toán Add
	    key = temp + SALT_VALUE;
	}
	return key;
    }
};

#endif //STANDARD_SECURITY_H



