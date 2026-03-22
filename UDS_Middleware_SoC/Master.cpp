#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <csignal>
#include <chrono>
#include <filesystem>

#include "FOTA_Manager.h"

namespace fs = std::filesystem;

// =================================================
// 1. KHÔNG GIAN ĐỒNG BỘ HÓA (SYNCHRONIZATION SPACE)
// =================================================
// atomic đọc/ghi cờ an toàn giữa các luồng mà không bị Race Condition
std::atomic<bool> g_keepRunning(true);
std::atomic<bool> g_fotaInProgress(false);

// Dùng Mutex để loại trừ lẫn nhau, tránh trường hợp FOTA đang chạy AI chen vào CAN
std::mutex g_canMutex;

// condition_variable gỡ thread AI khỏi CPU hoàn toàn
std::mutex g_aiWaitMutex;
std::condition_variable g_aiCv;

const std::string FIRMWARE_DIR = "Firmware/";

// Trỏ toàn cục trỏ đến FotaManager (nhận tham sớ init, không nhận object)
FotaManager* g_fotaManager = nullptr;

// =============================================
// 2. CÁC HÀM HỖ TRỢ LÕI (CORE HELPER FUNCTIONS)
// =============================================

/**
 * @brief Bắt tín hiệu ngắt từ hệ điều hành
 */
// Control Application when run underground "Systemctl"
void signalHandler(int signum) {
    std::cout << "[Master] Recived Interrupt Signal " << signum << ". Cleaning up...\n";
    g_keepRunning = false;
    g_aiCv.notify_all(); // Đánh thức AI (nếu nó đang ngủ chờ FOTA) để nó kịp kết thúc luồng
    if (g_fotaManager) {
        g_fotaManager->stop();
    }
}

/**
 * @brief Đứng chờ FOTA_Manager thực thi xong 1 lệnh UDS
 */
// Master cần phải chờ Response của một Service rồi mới thực thi Service khác
void waitFotaStep(FotaManager* fotaApp) {
    // Delay 50ms đợi nhịp CPU để workerThread bên trong fotaApp kịp gán biến isProcessing = true.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Chờ đến lúc workerThread còn đang báo bận. Trễ 100ms/vòng để không ăn CPU.
    while (fotaApp->isProcessing() && g_keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ==============================
// 3. LUỒNG TÁC VỤ PHỤ: AI WORKER
// ==============================
/*
void aiWorkerThread() {
    std::cout << "[AI Worker] Khởi động luồng AI...\n";

    while (g_keepRunning) {
        // Đây là "Trạm thu phí". Mỗi vòng lặp, AI đi qua đây.
        // Nếu g_fotaInProgress == true, hàm wait() khóa luồng này lại ngay lập tức.
        {
            std::unique_lock<std::mutex> lock(g_aiWaitMutex);
            g_aiCv.wait(lock, []{ return !g_fotaInProgress.load() || !g_keepRunning.load(); });
        }

        if (!g_keepRunning) break;

        // Bỏ code AI vào đây (Ví dụ: Thu âm thanh, chạy mô hình phát hiện tiếng trẻ em)
        // Giả lập AI tốn 500ms để phân tích 1 khung dữ liệu
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::cout << "[AI Worker] Đã thoát an toàn.\n";
}
*/

// ==================================
// 4. LUỒNG TÁC VỤ CHÍNH: FOTA DAEMON
// ==================================
void fotaWorkerThread(FotaManager* fotaApp) {
    std::cout << "[FOTA Worker] Starting automatic Monitoring...\n";

    if(!fs::exists(FIRMWARE_DIR)) {
	fs::create_directory(FIRMWARE_DIR);
    }

    while(g_keepRunning) {
	std::string zipPath = ""; // Biến lưu đường dẫn chứa package .zip

	for (const auto& entry : fs::directory_iterator(FIRMWARE_DIR)) {
	    if (entry.is_regular_file()) {
		// Nếu đuôi Folder chính xác là ".zip" thì lấy
		if (entry.path().extension() == ".zip") {
		    zipPath = entry.path().string();
                    break; // Tìm thấy 1 file là thoát vòng lặp quét ngay
		}
	    }
	}

	// Quét file thời gian thực. Download Package .tmp khi done thì .zip
	if (!zipPath.empty()) {
	    std::cout << "[FOTA Worker] NEW FIRMWARE [.ZIP] PACKAGE DETECTED: " << zipPath << std::endl;;

	    // Đảm bảo AI chạy khung data final và bị locked
	    g_fotaInProgress = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
	    bool isFotaSuccess = true; // Cờ theo dõi cả quá trình FOTA

	    // lock_guard sẽ tự động lock g_canMutex khi vào và unlock khi ra
	    {
	        std::lock_guard<std::mutex> canLock(g_canMutex);
                std::cout << "[FOTA Worker] Starting upload to VCU...\n";

	        // Bước 1: SESSION CONTROL - 0x10
	        fotaApp->startSessionProcess();
	        waitFotaStep(fotaApp);
	        // Ngắt lệnh (isFotaSuccess = false) nếu VCU reject
	        if (!fotaApp->getTaskResult()) {
                    std::cerr << "[Master] ABORT: Programming Session Error.\n";
                    isFotaSuccess = false;
                }

	        // Bước 2: SERCURITY ACCESS - 0x27
	        if (isFotaSuccess) {
                    fotaApp->startSecurityProcess();
                    waitFotaStep(fotaApp);
                    if (!fotaApp->getTaskResult()) {
		        std::cerr << "[Master] ABORT: Security Access Error.\n";
                        isFotaSuccess = false;
                    }
                }

	        // Bước 3: REQUEST DOWNLOAD - 0x34
	        if (isFotaSuccess) {
		    fotaApp->startRequestDownloadProcess(zipPath);
		    waitFotaStep(fotaApp);
		    if (!fotaApp->getTaskResult()) {
		        std::cerr << "[Master] ABORT: Service 0x34 Error or Invalid Firmware file.\n";
                        isFotaSuccess = false;
		    }
	        }

	        // Bước 4: TRANSFER DATA - 0x36
	        if (isFotaSuccess) {
		    fotaApp->startTransferDataProcess();
		    waitFotaStep(fotaApp);
		    if (!fotaApp->getTaskResult()) {
		        std::cerr << "[Master] ABORT: Connection lost during Data Transfer (0x36).\n";
		        isFotaSuccess = false;
		    }
	        }

	        // Bước 5: TRANSFER EXIT - 0x37
	        if (isFotaSuccess) {
		    fotaApp->startTransferExitProcess();
		    waitFotaStep(fotaApp);
		    if (!fotaApp->getTaskResult()) {
		        std::cerr << "[Master] ABORT: Service 0x37 failed. Hardware CRC mismatch.\n";
		        isFotaSuccess = false;
		    }
		}

	        // Bước 6: RESET ECU - 0x11
	        // FOTA thất bại (Firmware Error or mode Bootloader) Reset RollBack Active Bank
	        std::cout << "[FOTA Worker] Resetting VCU to complete session...\n";
	        fotaApp->startResetProcess();
                waitFotaStep(fotaApp);
	    }

	    // Xử lý vòng đời File, .done or .error (tránh while(1) khi thấy .zip)
	    std::string newPath = zipPath + (isFotaSuccess ? ".done" : ".error");
	    try {
	        fs::rename(zipPath, newPath);
	        std::cout << "[FOTA Worker] Finalized OTA file to: " << newPath << "\n";
	    }
	    catch (const fs::filesystem_error& e) {
	        std::cerr << "[FOTA Worker] Permission error while renaming file: " << e.what() << '\n';
	    }

	    // FOTA done. Đánh thức Thread AI
	    std::cout << "[FOTA Worker] Process complete. Waking up AI thread...\n";
	    g_fotaInProgress = false;
	    g_aiCv.notify_all();
        }
	// Thread FOTA ngủ 3s (tránh while(1)) chạy liên tục tốn CPU
	std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

// =============================
// 5. TRUNG TÂM KHỞI CHẠY (MAIN)
// =============================
int main(int argc, char* argv[]) {
    // Đăng ký bắt cờ tín hiệu từ Hệ điều hành
    signal(SIGINT, signalHandler);  // Bắt Ctrl+C (Khi debug)
    signal(SIGTERM, signalHandler); // Bắt lệnh kill từ systemctl (Khi chạy Production)

    std::cout << "  SoC DAEMON: AUTO FOTA & AI CONTROL CENTER \n";

    FotaManager fotaApp;
    g_fotaManager = &fotaApp;

    // Khởi tạo hạ tầng mạng. (SocketCAN)
    if (!fotaApp.initialize("can0", 0x7E0, 0x7E8)) {
        std::cerr << "[Master] CAN Network Error. System cannot start.\n";
        return -1;
    }

    // luồng AI và luồng FOTA chạy song song ở chế độ nền.
    //std::thread aiThread(aiWorkerThread);
    std::thread fotaThread(fotaWorkerThread, &fotaApp);

    // Luồng chính (main) lui về hậu trường, ngủ yên ở đây để giữ cho chương trình sống.
    // Nếu main() thoát, toàn bộ app sẽ bị hủy.
    while (g_keepRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Khi hệ thống nhận lệnh tắt, dùng join() đợi các luồng phụ dọn dẹp xong
    //if (aiThread.joinable()) aiThread.join();
    if (fotaThread.joinable()) fotaThread.join();

    std::cout << "[Master] Application Exited Successfully.\n";
    return 0;
}
