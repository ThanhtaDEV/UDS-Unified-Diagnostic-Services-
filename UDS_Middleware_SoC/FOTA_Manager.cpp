#include "FOTA_Manager.h"
#include <chrono>
#include "UdsConstants.h"

#include <fstream>
#include <zlib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

FotaManager::FotaManager()
    : transport(nullptr),
      client(nullptr),
      isRunning(false),
      isHeartbeatRunning(false)
{}

FotaManager::~FotaManager() {
    stop(); // Đảm bảo thread dừng trước khi hủy object
    // Dọn dẹp bộ nhớ theo thứ tự ngược lại lúc tạo
    if (client) delete client;
    if (transport) delete transport;
}

bool FotaManager::initialize(const std::string& interfaceName, uint32_t txId, uint32_t rxId) {
    std::cout << "[FOTA_App] Initializing FOTA Manager..." << std::endl;
    // Reset trạng thái an toàn
    isRunning = false;
    isHeartbeatRunning = false;

    // Dọn dẹp nếu đã từng init trước đó
    if (client) { delete client; client = nullptr; }
    if (transport) { delete transport; transport = nullptr; }

    try {
	// Gọi Constructor có tham số
        transport = new SocketCanTransport(interfaceName, txId, rxId);

	if (!transport->isValid()) {
        std::cerr << "[FOTA_App] FAILED: Could not connect to CAN interface!" << std::endl;
        delete transport; transport = nullptr;
        return false;
        }

        // Tạo Client và inject Transport vào
        client = new UdsClient(transport, nullptr, 2000);
	std::cout << "[FOTA_App] Transport Initialized on " << interfaceName
                  << " (Tx: 0x" << std::hex << txId << ", Rx: 0x" << rxId << ")" << std::endl;
        return true;
    }
    catch (const std::exception& e) {
	std::cerr << "[FOTA_App] FAILED to initialize Transport: " << e.what() << std::endl;
	return false;
    }
}

void FotaManager::stop() {
    // 1. Dừng Heartbeat trước (Quan trọng)
    stopHeartbeat();

    isRunning = false; // Báo hiệu cho vòng lặp (nếu có) dừng lại
    if(workerThread.joinable()) {
	workerThread.join(); // Chờ Thread kết thúc
    }
    std::cout << "[FOTA_App] FotaManager stopped." << std::endl;
}

bool FotaManager::isProcessing() const {
    return isRunning.load();
}
// Hàm trả về kết quả thành công/thất bại của Task hiện tại
bool FotaManager::getTaskResult() const {
    return m_taskSuccess.load();
}
// =========================================================
// SERVICE 1: ECU RESET (0x11)
// =========================================================
void FotaManager::startResetProcess() {
    // Dừng spam 0x3E để trả lại sự yên tĩnh cho mạng CAN
    stopHeartbeat();

    if (isRunning) {
        std::cout << "[FOTA_App - Start_Reset_ECU] System Busy!" << std::endl;
        return;
    }

    std::cout << "[FOTA_App - Start_Reset_ECU] Launching RESET Task." << std::endl;
    isRunning = true;

    if (workerThread.joinable()) {
        workerThread.join();
    }

    // Khởi chạy thread, trỏ thread vào hàm runResetTask
    workerThread = std::thread(&FotaManager::runResetTask, this);

    // Tách luồng này ra chạy ngầm, biến workerThread sẽ được reset về trạng thái trống
    workerThread.detach();
}

void FotaManager::runResetTask() {
    std::cout << "[FOTA_App - Run_Reset_ECU] RESET Task Started. " << std::endl;
    // Reset cờ thành false ngay từ đầu để đảm bảo an toàn
    m_taskSuccess = false;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // [QUAN TRỌNG] Trước khi Reset, phải TẮT Heartbeat
    // Vì VCU sắp khởi động lại, gửi 3E lúc này là vô nghĩa và gây nhiễu
    stopHeartbeat();

    try {
	std::cout << "[FOTA_App - Run_Reset_ECU] Sending VCU Hard Reset (0x11)." << std::endl;
	UdsResponse resp = client->requestHardReset();

        if (resp.isPositive()) {
	    std::cout << "[FOTA_App - Run_Reset_ECU] RESET SUCCESS!" << std::endl;
	    m_taskSuccess = true;
        } else {
            std::cerr << "[FOTA_App - Run_Reset_ECU] RESET FAILED! (NRC: 0x" << std::hex << (int)resp.getNRC() << ")" << std::endl;
	    m_taskSuccess = false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[FOTA_App - Run_Reset_ECU] Error in worker: " << e.what() << std::endl;
	m_taskSuccess = false;
    }

    isRunning = false;
    std::cout << "[FOTA_App - Run_Reset_ECU] Task Finished." << std::endl;
}

// =========================================================
// SERVICE 2: SESSION CONTROL (0x10)
// =========================================================
void FotaManager::startSessionProcess() {
    if (isRunning) {
        std::cout << "[FOTA_App - Start_Session_Control] System Busy!" << std::endl;
        return;
    }
    std::cout << "[FOTA_App - Start_Session_Control] Launching SESSION Task." << std::endl;
    isRunning = true;

    // Trỏ thẳng thread vào hàm runSessionTask
    workerThread = std::thread(&FotaManager::runSessionTask, this);

    // Tách luồng này ra chạy ngầm, biến workerThread sẽ được reset về trạng thái trống
    workerThread.detach();
}

void FotaManager::runSessionTask() {
    std::cout << "[FOTA_App - Run_Session] SESSION Task Started." << std::endl;
    // Reset cờ thành false ngay từ đầu để đảm bảo an toàn
    m_taskSuccess = false;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    try {
        std::cout << "[FOTA_App - Run_Session] Sending SessionControl (Send Key)..." << std::endl;
        UdsResponse resp = client->requestSession(Uds::Session::Programming);

        if (resp.isPositive()) {
            std::cout << "[FOTA_App - Run_Session] SESSION changed to PROGRAMMING." << std::endl;
	    // [QUAN TRỌNG] Vào Programming thành công thì phải BẬT Heartbeat ngay
            // Để VCU không tự thoát ra sau 5 giây (S3 Server Timeout)
            startHeartbeat();
	    m_taskSuccess = true;
        }
	else {
            std::cerr << "[FOTA_App - Run_Session] SESSION change Failed! (NRC: 0x" << std::hex << (int)resp.getNRC() << ")" << std::endl;
	    m_taskSuccess = false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[FOTA_App - Run_Session] Error: " << e.what() << std::endl;
	m_taskSuccess = false;
    }

    isRunning = false;
    std::cout << "[FOTA_App - Run_Session] SESSION Task Finished." << std::endl;
}

// =========================================================
// SERVICE 3: TESTER PRESENT (0x3E)
// =========================================================
void FotaManager::startHeartbeat() {
    if (isHeartbeatRunning) return; // Đang chạy rồi thì thôi

    // Dọn thread cũ nếu còn sót
    if (heartbeatThread.joinable()) heartbeatThread.join();

    std::cout << "[FOTA_App - Start_Tester_Present] STARTING HEARTBEAT (Tester Present)." << std::endl;
    isHeartbeatRunning = true;
    heartbeatThread = std::thread(&FotaManager::runHeartbeatTask, this);
}

void FotaManager::stopHeartbeat() {
    if (!isHeartbeatRunning) return;

    std::cout << "[FOTA_App - Stop_Tester_Present] STOPPING HEARTBEAT." << std::endl;
    isHeartbeatRunning = false; // Hạ cờ để vòng lặp thoát

    if (heartbeatThread.joinable()) {
        heartbeatThread.join();
    }
}

void FotaManager::runHeartbeatTask() {
    while (isHeartbeatRunning) {
        // Gửi lệnh 3E 80 (Fire & Forget)
        if (client) client->sendTesterPresent();

        // Ngủ 2 giây (Standard S3 time)
        // Chia nhỏ giấc ngủ để phản hồi nhanh khi lệnh Stop được gọi
        for (int i = 0; i < 20; ++i) {
            if (!isHeartbeatRunning) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// =========================================================
// SERVICE 4: SECURITY ACCESS (0x27)
// =========================================================
void FotaManager::startSecurityProcess() {
    if (workerThread.joinable()) workerThread.join();

    if (isRunning) {
        std::cout << "[FOTA_App - Start_SecurityAccess] System Busy!" << std::endl;
        return;
    }

    std::cout << "[FOTA_App - Start_SecurityAccess] Launching SECURITY Task." << std::endl;
    isRunning = true;

    // Khởi chạy thread, trỏ thread vào hàm runSecurityTask
    workerThread = std::thread(&FotaManager::runSecurityTask, this);

    // Tách luồng này ra chạy ngầm, biến workerThread sẽ được reset về trạng thái trống
    workerThread.detach();
}

void FotaManager::runSecurityTask() {
    std::cout << "[FOTA_App - Run_SecurityAccess] SECURITY ACCESS Task Started." << std::endl;
    // Reset cờ thành false ngay từ đầu để đảm bảo an toàn
    m_taskSuccess = false;

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    try {
	bool success = client->unlockSecurity(Uds::Security::RequestSeed);

	if (success) {
	    std::cout << "[FOTA_App - Run_SecurityAccess] Security Access Flow Completed Successfully." << std::endl;
	    m_taskSuccess = true;
        }
        else {
            std::cerr << "[FOTA_App - Run_SecurityAccess] Security Access Flow FAILED." << std::endl;
	    m_taskSuccess = false;
        }
    }
    catch (const std::exception& e) {
	std::cerr << "[FOTA_App - Run_SecurityAccess] Exception caught: " << e.what() << std::endl;
        m_taskSuccess = false;
    }

    isRunning = false;
    std::cout << "[FOTA_App - Run_SecurityAccess] Task Finished." << std::endl;
}

// ============================================================================
// HELPER 1: GIẢI NÉN FILE ZIP BẰNG LỆNH HỆ THỐNG LINUX
// ============================================================================
bool FotaManager::unzipFirmware(const std::string& zipFilePath, const std::string& outputDir) {
    std::cout << "[FOTA_App] Unzipping: " << zipFilePath << " ...\n";

    // Dọn dẹp thư mục cũ và tạo thư mục mới
    std::string cleanCmd = "rm -rf " + outputDir + " && mkdir -p " + outputDir;
    std::system(cleanCmd.c_str());

    // Giải nén (-o: overwrite, -q: quiet mode)
    std::string unzipCmd = "unzip -o -q " + zipFilePath + " -d " + outputDir;
    int ret = std::system(unzipCmd.c_str());

    if (ret != 0) {
        std::cerr << "[FOTA_App] <ERROR> Failed to unzip file! Is the path correct?\n";
        return false;
    }
    return true;
}

// ============================================================================
// HELPER 2: ĐỌC VÀ TRÍCH XUẤT THÔNG TIN TỪ MANIFEST.JSON
// ============================================================================
bool FotaManager::parseManifest(const std::string& manifestPath) {
    std::cout << "[FOTA_App] Parsing manifest.json...\n";

    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        std::cerr << "[FOTA_App] <ERROR> Cannot open manifest.json at " << manifestPath << "\n";
        return false;
    }

    try {
        json manifest = json::parse(file);

        // Lấy thông tin theo format do Python sinh ra
        std::string addrStr = manifest["firmware"]["flash_address"];
        std::string crcStr  = manifest["firmware"]["package_file_crc32"];
        std::string binName = manifest["firmware"]["file_name"];

        m_otaSizeBytes      = manifest["firmware"]["ota_size_bytes"];

        // Ép kiểu Hex String sang uint32_t
        m_targetAddress = std::stoul(addrStr, nullptr, 16);
        m_expectedCrc32 = std::stoul(crcStr, nullptr, 16);

        // Lưu lại đường dẫn tới file bin thực tế để tính CRC và gửi 0x36 sau này
        m_binFilePath = m_extractDir + "/" + binName;

        std::cout << "   -> Target Address: 0x" << std::hex << m_targetAddress << "\n";
        std::cout << "   -> OTA Size: " << std::dec << m_otaSizeBytes << " bytes\n";
        std::cout << "   -> Expected CRC32: 0x" << std::hex << m_expectedCrc32 << std::dec << "\n";

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[FOTA_App] <ERROR> JSON Parsing failed: " << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// HELPER 3: TÍNH TOÁN CRC32 CỦA FILE (Chuẩn STM32F4 Hardware CRC
// ============================================================================
uint32_t FotaManager::calculateFileCRC32(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[FOTA_App] <ERROR> CRC Check: Cannot open file " << filePath << "\n";
        return 0;
    }

    uint32_t crc = crc32(0L, Z_NULL, 0);
    char buffer[4096];

    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        crc = crc32(crc, reinterpret_cast<const Bytef*>(buffer), file.gcount());
    }
    return crc;
}

// =========================================================
// SERVICE 5: REQUEST DOWNLOAD (0x34)
// =========================================================
void FotaManager::startRequestDownloadProcess(const std::string& zipFilePath) {
    if (isRunning.load()) {
        std::cerr << "[FOTA_App - Start_RequestDownload] <WWARNING> A process is already running!\n";
        return;
    }

    isRunning.store(true);
    m_zipFilePath = zipFilePath;
    m_extractDir = "Firmware/extracted"; // Cố định thư mục giải nén

    if (workerThread.joinable()) {
        workerThread.join();
    }

    // Khởi chạy task trong Thread mới
    workerThread = std::thread(&FotaManager::runRequestDownloadTask, this);

    // Tách luồng này ra chạy ngầm, biến workerThread sẽ được reset về trạng thái trống
    workerThread.detach();
}

void FotaManager::runRequestDownloadTask() {
    std::cout << "[FOTA_App - Run_RequestDownload] STARTING FIRMWARE VERIFICATION & 0x34 \n";
    // Reset cờ thành false ngay từ đầu để đảm bảo an toàn
    m_taskSuccess = false;

    // BƯỚC 1: Giải nén
    if (!unzipFirmware(m_zipFilePath, m_extractDir)) {
        isRunning.store(false); return;
    }

    // BƯỚC 2: Đọc file JSON
    std::string manifestPath = m_extractDir + "/manifest.json";
    if (!parseManifest(manifestPath)) {
        isRunning.store(false); return;
    }

    // BƯỚC 3: Verify CRC32 của file Firmware
    std::cout << "[FOTA_App - Run_RequestDownload] Caculating CRC32 For: " << m_binFilePath << " ...\n";
    uint32_t actualCrc32 = calculateFileCRC32(m_binFilePath);

    std::cout << "[Verify] Expected CRC (from JSON) : 0x" << std::hex << m_expectedCrc32 << "\n";
    std::cout << "[Verify] Actual CRC (calculated)  : 0x" << std::hex << actualCrc32 << std::dec << "\n";

    if (actualCrc32 != m_expectedCrc32) {
	std::cerr << "[Verify] Result: MISMATCH! \n";
        std::cerr << "[FOTA_App - Run_RequestDownload] CRITICAL ERROR: The Firmware file is corrupted or invalid!\n";
        std::cerr << "[FOTA_App - Run_RequestDownload] ABORTING FOTA PROCESS! (Request Download - 0x34 will not be sent). \n";
        isRunning.store(false);
        return; // Dừng lập tức, không cho phép gọi 0x34
    }
    std::cerr << "[Verify] Result: MATCH! Firmware is perfectly valid. \n";

    // BƯỚC 4: File an toàn 100%, tiến hành đàm phán với VCU (Lệnh 0x34)
    std::cout << "[FOTA_App - Run_RequestDownload] Sending 0x34 Request Download to VCU...\n";
    std::cout << "[FOTA_App - Run_RequestDownload] Target Address: 0x" << std::hex << m_targetAddress << ", Size: " << std::dec << m_otaSizeBytes << " bytes.\n";

    try {
	// Gọi API của UdsClient, "trái tim" sẽ tự động bắt vòng lặp 0x78 nếu VCU bận xóa Flash
	uint32_t blockLen = client->requestDownload(m_targetAddress, m_otaSizeBytes);

	if (blockLen > 0) {
	    m_maxBlockLength = blockLen - 2;
            std::cout << "[FOTA_App - Run_RequestDownload] 0x34 SUCCESS! VCU is ready to receive data.\n";
	    std::cout << "[FOTA_App - Run_RequestDownload] Raw Block Length from VCU: " << std::dec << blockLen << " bytes.\n";
            std::cout << "[FOTA_App - Run_RequestDownload] Actual Payload Size agreed: " << std::dec << m_maxBlockLength << " bytes.\n";

	    m_taskSuccess = true;
        }
	else {
            std::cerr << "[FOTA_App - Run_RequestDownload] 0x34 FAILED! VCU rejected or returned invalid block length.\n";
	    // Cố tình gán m_maxBlockLength = 0 để hàm 0x36 phía sau check thấy lỗi và tự hủy
            m_maxBlockLength = 0;

	    m_taskSuccess = false;
        }
    }
    catch (const std::exception& e) {
	std::cerr << "[FOTA_App - Run_RequestDownload] Exception caught: " << e.what() << "\n";
        m_taskSuccess = false; // Bắt lỗi an toàn
    }

    // Kết thúc Thread
    std::cout << "[FOTA_App - Run_RequestDownload] Request Download Task Finished.\n";
    isRunning.store(false);
}

// =========================================================
// SERVICE 6: TRANSFER DATA (0x36)
// =========================================================
void FotaManager::startTransferDataProcess() {
    std::cout << "[FOTA_App - Start_TransferData] Activating Transfer Data process (0x36).\n";
    if (m_binFilePath.empty()) {
        std::cerr << "[FOTA_App - Start_TransferData] <ERROR> .bin file path is empty! Serivce 0x34 incomplete or decompression failed.\n";
        return; // Chặn đứng ngay lập tức!
    }

    if (m_maxBlockLength == 0) {
        std::cerr << "[FOTA_App - Start_TransferData] <ERROR> m_maxBlockLength = 0! VCU has not responded with allowed block size.\n";
        return;
    }

    // Khoá an toàn luồng
    if (isRunning.load()) {
        std::cerr << "[FOTA_App - Start_TransferData] <WARNING> Another FOTA process is running! Cannot start Service 0x36.\n";
        return;
    }

    // Bật cờ báo hiệu hệ thống đang bận
    isRunning.store(true);

    // Dọn dẹp luồng cũ (luồng 0x34 vừa chạy xong)
    if (workerThread.joinable()) {
        workerThread.join();
    }

    stopHeartbeat();

    // BƯỚC ĐẨY LUỒNG MỚI
    std::cout << "[FOTA_App - Start_TransferData] Spawning background thread to split File: " << m_binFilePath << "\n";
    workerThread = std::thread(&FotaManager::runTransferDataTask, this);
}

void FotaManager::runTransferDataTask() {
    std::cout <<"[FOTA_App - Run_TransferData] Staring 0x36 - Tranfer Data\n";
    m_taskSuccess = false;

    std::ifstream binFile(m_binFilePath, std::ios::binary);
    if (!binFile.is_open()) {
        std::cerr << "[FOTA_App - Run_TransferData] <CRITICAL ERROR> Cannot open File " << m_binFilePath << "\n";
        isRunning.store(false);
        return;
    }

    // Khởi tạo sổ sách
    uint8_t blockCounter = 1; // UDS bắt buộc gói đầu tiên là 1
    uint32_t totalBytesSent = 0;

    // Cài đặt "Kích thước" đã chốt từ lệnh 0x34
    std::vector<uint8_t> buffer(m_maxBlockLength);

    try {
	// LÝ DO 2: Vòng lặp băm file (Flashing)
	while (binFile) {
            // Cắt 1 block raw để chuẩn bị flash
            binFile.read(reinterpret_cast<char*>(buffer.data()), m_maxBlockLength);

            // LÝ DO 3: Dùng gcount() là BẮT BUỘC
            // Giả sử m_payloadSizePerBlock = 1000. File có 2500 bytes.
            // Vòng 1: Cắt 1000. Vòng 2: Cắt 1000. Vòng 3: Chỉ còn dư 500 bytes!
            // gcount() sẽ trả về đúng 500. Nếu không có gcount, sẽ gửi nhầm 500 bytes rác cũ đi.
            std::streamsize bytesRead = binFile.gcount();

            if (bytesRead == 0) {
                break; // Hết file, hoàn thành nhiệm vụ!
            }

            // Tạo mảng mới CHỈ CHỨA ĐÚNG số byte vừa cắt thực tế (bytesRead)
            std::vector<uint8_t> actualChunk(buffer.begin(), buffer.begin() + bytesRead);

            // LÝ DO 4: Giao hàng và Xử lý lỗi tức thời
            // Gọi Platform đẩy xuống mạng CAN
            bool success = client->transferData(blockCounter, actualChunk);

            if (!success) {
                std::cerr << "[FOTA_App - Run_TransferData] ERROR in BLOCK 0x" << std::hex << (int)blockCounter
                          << ". VCU rejected request or connection lost. Cancel FOTA!\n";
                binFile.close();
                isRunning.store(false);
                return; // Đứt gánh giữa đường, thoát luồng ngay!
            }

            // LÝ DO 5: Thanh tiến trình và Roll-over
            totalBytesSent += bytesRead;
            // Dùng \r để in đè, Tạo hiệu ứng % chạy mượt mà trên Terminal
	    // Tính toán phần trăm (Ép kiểu float để chia không bị làm tròn về 0)
            float percent = static_cast<float>(totalBytesSent) / m_otaSizeBytes;

	    // Độ dài của thanh bar trên màn hình Terminal
            int barWidth = 100;
            int pos = static_cast<int>(barWidth * percent);
	    // Bắt đầu vẽ ghi đè lên dòng hiện tại (\r)
            std::cout << "\r[Progress 0x36] [";
            for (int i = 0; i < barWidth; ++i) {
                if (i < pos) {
                    std::cout << "="; // Phần đã chạy xong
                }
                else if (i == pos) {
                    std::cout << ">"; // Mũi tên dẫn đầu
                }
                else {
                    std::cout << " "; // Phần chưa chạy tới
                }
            }

	    // In phần trăm số và số byte ở đuôi, VÀ BẮT BUỘC PHẢI CÓ std::flush
            std::cout << "] " << static_cast<int>(percent * 100.0) << "% ("
                      << totalBytesSent << "/" << m_otaSizeBytes << " bytes)"
                      << std::flush;

            // Tự động tràn: Khi blockCounter = 255 (0xFF), lệnh ++ này sẽ đưa về 0 (0x00).
            // Đây là tính năng của kiểu uint8_t, đáp ứng hoàn hảo chuẩn đếm vòng của UDS.
            blockCounter++;
        }

	m_taskSuccess = true;
        // Dọn dẹp chiến trường
        std::cout << "\n[FOTA_App - Run_TransferData] Service 0x36 SUCCESS! 100% of Data Successfully uploaded to VCU.\n";
    }
    catch (const std::exception& e) {
	std::cerr << "[FOTA_App - Run_TransferData] Exception: " << e.what() << "\n";
        m_taskSuccess = false;
    }

    binFile.close();
    startHeartbeat();
    // Hạ cờ, báo hiệu luồng đã rảnh rỗi
    isRunning.store(false);
}


// =========================================================
// SERVICE 7: TRANSFER EXIT (0x37)
// =========================================================
void FotaManager::startTransferExitProcess() {
    std::cout << "[FOTA_App - Start_TransferExit] Transfer Exit (0x37) Starting\n";

    // Khoá an toàn luồng: Đảm bảo luồng 0x36 đã thực sự nhường sân
    if (isRunning.load()) {
        std::cerr << "[FOTA_App - Start_TransferExit] <WARNING> Another FOTA process is already running! Cannot start 0x37.\n";
        return;
    }

    // Bật cờ báo hiệu hệ thống đang bận cho tác vụ 0x37
    isRunning.store(true);

    // Dọn dẹp cái luồng cũ (luồng 0x36 vừa chạy xong)
    if (workerThread.joinable()) {
        workerThread.join();
    }

    // Đảm bảo Heartbeat đã tắt để chuẩn bị chốt sổ và Reset
    stopHeartbeat();

    // BƯỚC ĐẨY LUỒNG MỚI
    std::cout << "[FOTA_App - Start_TransferExit] Spawning background thread to finalize Service 0x37.\n";
    workerThread = std::thread(&FotaManager::runTransferExitTask, this);
}

void FotaManager::runTransferExitTask() {
    std::cout << "[FOTA_App - Run_TransferExit] Requesting VCU to perform Hardware CRC scan and write Metadata.\n";
    m_taskSuccess = false;

    try {
        // Gọi API từ UdsClient (đã được bọc try-catch nội bộ)
        bool isSuccess = client->requestTransferExit();

        if (isSuccess) {
            std::cout << "[FOTA_App - Run_TransferExit] <OK> Command 0x37 send Successfully!\n";
            std::cout << "[FOTA_App - Run_TransferExit] VCU verified CRC and marked BANK_STATE_VALID\n";

	    m_taskSuccess = true;
            // (Tùy chọn) Bạn có thể tự động trigger lệnh Reset tại đây nếu muốn luồng chạy liên tục:
            // startResetProcess();
        }
	else {
            std::cerr << "[FOTA_App - Run_TransferExit] <ERROR> Service 0x37 FAILED! VCU rejected finalize command (CRC Mismatch or Flash Error).\n";
	    m_taskSuccess = false;
        }
    }
    catch (const std::exception& e) {
	std::cerr << "[FOTA_App - Run_TransferExit] Exception: " << e.what() << "\n";
        m_taskSuccess = false;
    }

    // CỰC KỲ QUAN TRỌNG
    // Tiến trình đã xong (dù Pass hay Fail), phải hạ cờ xuống
    // để các hàm start...() tiếp theo (như startResetProcess) có thể vượt qua hàm if(isRunning.load())
    startHeartbeat();
    isRunning.store(false);
    std::cout << "[FOTA_App - Run_TransferExit] Service 0x37 completed and self-released.\n";
}

