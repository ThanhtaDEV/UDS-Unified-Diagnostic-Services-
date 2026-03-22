#ifndef FOTA_MANAGER_H
#define FOTA_MANAGER_H

#include <iostream>
#include <thread>
#include <atomic>
#include <string>

#include <atomic> // cờ nguyên tử

// Include các thành phần lớp dưới
#include "UdsClient.h"
#include "SocketCanTransport.h"

class FotaManager {
public:
    FotaManager();
    ~FotaManager();
    /**
     * @brief Khởi tạo kết nối CAN và các thành phần
     * @param interfaceName Tên cổng CAN (vd: "can0")
     * @param txId CAN ID gửi đi (SoC -> VCU, vd: 0x7E0)
     * @param rxId CAN ID nhận về (VCU -> SoC, vd: 0x7E8)
     */
    bool initialize(const std::string& interfaceName, uint32_t txId, uint32_t rxId);

    // Session Control (Ox10)
    void startSessionProcess(); // Call API from main

    // Reset ECU (0x11)
    void startResetProcess();	// Call API form main

    // Security Access (0x27)
    void startSecurityProcess(); // Call API form main

    /**
     * @brief Đọc manifest.json và gửi yêu cầu Request Download (0x34)
     * @param updateDir Thư mục chứa manifest.json
     */
    void startRequestDownloadProcess(const std::string& zipFilePath);

    /**
     * @brief Đọc file .bin, cắt nhỏ và gửi liên tục qua Transfer Data (0x36)
     * Lệnh này chỉ được gọi SAU KHI 0x34 đã báo thành công.
     */
    void startTransferDataProcess();

    void startTransferExitProcess();

    /**
     * @brief Dừng thread và dọn dẹp tài nguyên
     */
    void stop();

    /**
     * @brief Kiểm tra xem process có đang chạy không
     */
    bool isProcessing() const;

    // Để Master đọc kết quả sau khi xử lý xong
    bool getTaskResult() const;

private:
    // Composition Objects (Sở hữu các object lớp dưới)
    SocketCanTransport* transport;
    UdsClient* client;

    // Threading Management
    std::thread workerThread;      // Luồng xử lý công việc nặng
    std::atomic<bool> isRunning;   // Cờ báo hiệu trạng thái luồng

    // Lưu kết quả (True = Positive Response, False = Error/Timeout)
    std::atomic<bool> m_taskSuccess{false};

    // Heartbeat Thread (Chạy ngầm Tester Present 0x3E)
    std::thread heartbeatThread;
    std::atomic<bool> isHeartbeatRunning;

    // Cờ báo hiệu đang bận truyền dữ liệu 0x36. Mặc định là false.
    std::atomic<bool> m_isTransferring{false};

    // Biến chia sẻ (Shared State) cho FOTA
    std::string m_extractDir;     // Nơi chứa file sau khi giải nén
    std::string m_binFilePath;    // Đường dẫn tới file _ota.bin
    std::string m_zipFilePath;    // Lưu lại đường dẫn zip đầu vào

    // Các biến lưu thông tin file OTA (Đọc từ JSON)
    uint32_t m_targetAddress;	// Đọc từ JSON
    uint32_t m_otaSizeBytes;	// Đọc từ JSON
    uint32_t m_expectedCrc32;   // Đọc từ JSON (package_file_crc32)
    uint32_t m_maxBlockLength;	// Lưu kết quả do VCU trả về ở lệnh 0x34

    // Helper Functions cho FOTA
    bool unzipFirmware(const std::string& zipFilePath, const std::string& outputDir);
    bool parseManifest(const std::string& manifestPath);
    uint32_t calculateFileCRC32(const std::string& filePath); // Hàm tính CRC32

    // Nhiệm vụ
    void runResetTask();	// 0x11
    void runSessionTask();	// 0x10
    void runSecurityTask();	// 0x27
    // Logic Heartbeat (Gửi 3E 80 định kỳ)
    void runHeartbeatTask();	// 0x3E

    // Helper bật/tắt Heartbeat nội bộ (0x3E)
    void startHeartbeat();
    void stopHeartbeat();

    void runRequestDownloadTask(); // 0x34
    void runTransferDataTask();    // 0x36
    void runTransferExitTask(); // 0x37
};

#endif // FOTA_MANAGER_H
