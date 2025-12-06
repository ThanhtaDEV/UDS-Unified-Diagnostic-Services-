#include <iostream>
#include <fstream>
#include <string>
#include <vector>

// Include các module
#include "MockTransport.h"
#include "UdsClient.h"
#include "UdsException.h"
#include "TestParser.h" // Bộ phân tích chúng ta vừa tạo

int main() {
    // 1. Setup hệ thống
    MockTransport mock;
    UdsClient client(&mock);

    // 2. Mở file kịch bản (nằm ở thư mục stubs)
    // Lưu ý: Đường dẫn "../stubs/" vì ta chạy file từ trong thư mục src hoặc build
    std::ifstream reqFile("../stubs/requests.txt");
    std::ifstream resFile("../stubs/responses.txt");

    if (!reqFile.is_open() || !resFile.is_open()) {
        std::cerr << "ERROR: Cannot open test files in ../stubs/" << std::endl;
        return -1;
    }

    std::cout << "=== STARTING DATA-DRIVEN TEST ===\n" << std::endl;

    std::string reqLine, resLine;
    int lineCount = 0;

    // 3. Vòng lặp Test Loop: Đọc từng dòng song song
    while (std::getline(reqFile, reqLine) && std::getline(resFile, resLine)) {
        lineCount++;
        
        // Bỏ qua dòng trống hoặc comment (#)
        if (reqLine.empty() || reqLine[0] == '#') continue;

        std::cout << "--- Test Case #" << lineCount << " ---" << std::endl;

        // B3.1: Phân tích dữ liệu từ file text
        auto request = TestParser::parseRequestLine(reqLine); // {Lệnh, Tham số}
        auto responseData = TestParser::parseHexString(resLine); // {Byte Hex}

        // B3.2: NẠP ĐẠN cho Mock Transport (Giả lập ECU sẽ trả lời thế này)
        mock.pushResponse(responseData);

        // B3.3: Thực thi Client (Gọi API dựa trên từ khóa)
        std::string command = request.first;
        int param = request.second;

        try {
            UdsResponse resp(std::vector<uint8_t>{}); // Biến tạm

            if (command == "SESSION") {
                resp = client.requestSession((uint8_t)param);
            } 
            else if (command == "RESET") {
                resp = client.requestHardReset();
            }
            // Sau này thêm SECURITY, DOWNLOAD... ở đây dễ dàng

            // B3.4: In kết quả
            std::cout << "-> RESULT: PASS (Got Positive Response)\n" << std::endl;

        } catch (const UdsException& e) {
            // Nếu file response là 7F (Negative), code sẽ nhảy vào đây
            std::cout << "-> RESULT: EXPECTED ERROR (Caught: " << e.what() << ")\n" << std::endl;
        }
    }

    std::cout << "=== ALL TESTS COMPLETED ===" << std::endl;
    return 0;
}
