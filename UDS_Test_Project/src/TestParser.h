#ifndef TESTPARSER_H
#define TESTPARSER_H

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iomanip>

class TestParser {
public:
    // HÀM 1: Chuyển chuỗi Hex "50 03 00" thành vector {0x50, 0x03, 0x00}
    static std::vector<uint8_t> parseHexString(const std::string& line) {
        std::vector<uint8_t> data;
        std::stringstream ss(line);
        std::string byteStr;

        // Bỏ qua các dòng comment bắt đầu bằng # hoặc dòng trống
        if (line.empty() || line[0] == '#') return {};

        while (ss >> byteStr) {
            try {
                // Chuyển string hex sang int, rồi ép kiểu về byte
                data.push_back(static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16)));
            } catch (...) {
                // Nếu gặp ký tự không phải hex (ví dụ comment cuối dòng), dừng lại
                break;
            }
        }
        return data;
    }

    // HÀM 2: Phân tích dòng lệnh Request (Ví dụ: "SESSION 03")
    // Trả về: pair<Tên Lệnh, Tham số>
    static std::pair<std::string, int> parseRequestLine(const std::string& line) {
        std::stringstream ss(line);
        std::string command;
        std::string paramStr;
        
        ss >> command; // Lấy từ đầu tiên (SESSION, RESET...)
        
        if (ss >> paramStr) {
            try {
                // Tham số thường là hex (03, 01...)
                return {command, std::stoi(paramStr, nullptr, 16)};
            } catch (...) {
                return {command, 0};
            }
        }
        return {command, 0}; // Lệnh không có tham số
    }
};

#endif // TESTPARSER_H
