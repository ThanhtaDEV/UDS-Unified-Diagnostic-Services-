// UdsException.cpp
#include "UdsException.h"
#include <sstream>
#include <iomanip>

UdsException::UdsException(uint8_t nrcCode) : nrc(nrcCode) {
    // Tạo thông báo lỗi đầy đủ: "NRC 0x33: Security Access Denied"
    std::stringstream ss;
    ss << "UDS Error [NRC: 0x" 
       << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)nrc 
       << "] - " << getNrcDescription(nrc);
    message = ss.str();
}

const char* UdsException::what() const noexcept {
    return message.c_str();
}

uint8_t UdsException::getNRC() const {
    return nrc;
}

// TỪ ĐIỂN ÁNH XẠ MÃ LỖI (Mapping)
std::string UdsException::getNrcDescription(uint8_t nrc) {
    switch (nrc) {
        case 0x10: return "General Reject";
        case 0x11: return "Service Not Supported";
        case 0x12: return "Sub-function Not Supported";
        case 0x13: return "Incorrect Message Length Or Invalid Format";
        case 0x14: return "Response Too Long";
        case 0x21: return "Busy Repeat Request";
        case 0x22: return "Conditions Not Correct";
        case 0x24: return "Request Sequence Error";
        case 0x31: return "Request Out Of Range";
        case 0x33: return "Security Access Denied";
        case 0x35: return "Invalid Key";
        case 0x36: return "Exceed Number Of Attempts";
        case 0x72: return "General Programming Failure";
        case 0x73: return "Wrong Block Sequence Counter";
        case 0x78: return "Response Pending"; // (Quan trọng: Đang xử lý, chờ chút)
        case 0x7E: return "Sub-function Not Supported In Active Session";
        case 0x7F: return "Service Not Supported In Active Session";
	case 0xFF: return "Client Timeout (No Response from ECU)"; // Lỗi Timeout
	case 0xFE: return "Protocol Error (Wrong SID/Format)";    // Lỗi Logic
        default:   return "Unknown NRC";
    }
}
