#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <unistd.h> // cho sleep, getopt
#include <filesystem>

#include "FOTA_Manager.h"
#include "UdsConstants.h"

// --- CẤU HÌNH MẶC ĐỊNH ---
std::string g_canInterface = "can0";
uint32_t g_txId = 0x7E0; // SoC gửi
uint32_t g_rxId = 0x7E8; // SoC nhận

// Global pointer để signal handler truy cập
FotaManager* g_fotaManager = nullptr;
std::atomic<bool> g_keepRunning(true);

/**
 * @brief Bắt tín hiệu Ctrl+C để dừng chương trình an toàn
 */
void signalHandler(int signum) {
    std::cout << "\n[Main] Caught signal " << signum << ". Shutting down..." << std::endl;
    g_keepRunning = false;
    if (g_fotaManager) {
        g_fotaManager->stop();
    }
}

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [-i can_interface] [-t tx_id] [-r rx_id]\n"
              << "  -i : CAN interface name (default: can0)\n"
              << "  -t : Tx CAN ID in hex (default: 7E0)\n"
              << "  -r : Rx CAN ID in hex (default: 7E8)\n"
              << "Example: " << progName << " -i can1 -t 7E0 -r 7E8\n";
}

void parseArgs(int argc, char* argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "i:t:r:h")) != -1) {
        switch (opt){
            case 'i':
                g_canInterface = optarg;
                break;
            case 't':
                g_txId = std::strtoul(optarg, nullptr, 16);
                break;
            case 'r':
                g_rxId = std::strtoul(optarg, nullptr, 16);
                break;
            case 'h':
                printUsage(argv[0]);
                exit(0);
            default:
                printUsage(argv[0]);
                exit(1);
        }
    }
}

void printMenu() {
    std::cout << "\n----------------------------------------\n";
    std::cout << "          SoC FOTA CONTROL MENU         \n";
    std::cout << "----------------------------------------\n";
    std::cout << "  [s] : Enter Programming Session (0x10 0x02)" << std::endl;
    std::cout << "  [u] : Unlock Security Access (0x27)\n";
    std::cout << "  [f] : Start Firmware Update (Unzip, Verify & 0x34)\n";
    std::cout << "  [t] : Start Transfer Data (0x36)\n";
    std::cout << "  [e] : Request Transfer Exit (0x37)\n";
    std::cout << "  [r] : Request ECU Hard Reset (0x11)\n";
    std::cout << "  [q] : Quit Application\n";
    std::cout << "----------------------------------------\n";
    std::cout << "Enter command: ";
}

int main(int argc, char* argv[]) {
    // 1. Xử lý tham số đầu vào
    parseArgs(argc, argv);

    // 2. Đăng ký Signal Handler (Ctrl+C)
    signal(SIGINT, signalHandler);

    std::cout << "========================================\n";
    std::cout << "      SoC FOTA Client Application       \n";
    std::cout << "========================================\n";
    std::cout << "Interface : " << g_canInterface << "\n";
    std::cout << "Tx ID     : 0x" << std::hex << g_txId << "\n";
    std::cout << "Rx ID     : 0x" << g_rxId << std::dec << "\n";

    // 3. Khởi tạo FotaManager
    FotaManager fotaApp;
    g_fotaManager = &fotaApp; // Gán vào global để signal handler dùng

    if (!fotaApp.initialize(g_canInterface, g_txId, g_rxId)) {
        std::cerr << "[Main] Error: Failed to initialize FOTA Manager.\n";
        return -1;
    }

    // 4. Vòng lặp chính (Main Loop)
    while (g_keepRunning) {
        printMenu();

        char cmd;
        std::cin >> cmd;

        // Xử lý trường hợp Ctrl+D (EOF) hoặc lỗi input
        if (std::cin.fail()) {
            break;
        }

        switch (cmd) {
	    case 'u':
	    case 'U':
		if(!fotaApp.isProcessing()) {
		    std::cout << "[Main] Requesting Security Unlock...\n";
                    fotaApp.startSecurityProcess();
                } else {
                    std::cout << "[Main] System is busy! Please wait.\n";
                }
                break;

	    case 's':
	    case 'S':
                if (!fotaApp.isProcessing()) {
                    std::cout << "[Main] Requesting Programming Session (0x10 0x02)...\n";
                    fotaApp.startSessionProcess();
                } else {
                    std::cout << "[Main] Warning: Cannot start Session. ECU is busy with another task!\n";
                }
                break;

            case 'r':
            case 'R':
                if (!fotaApp.isProcessing()) {
		    std::cout << "[Main] Requesting ECU Hard Reset (0x11)...\n";
                    fotaApp.startResetProcess();
                } else {
                    std::cout << "[Main] Warning: Cannot start Reset. ECU is busy with another task!\n";
                }
                break;

	    case 'f':
            case 'F':
	    {
		std::string zipPath = "";
    		std::string targetDir = "Firmware";
		std::cout << "[Main] Đang quét tìm file .zip trong thư mục " << targetDir << "...\n";

		// Kiểm tra xem thư mục có tồn tại không
    		if (std::filesystem::exists(targetDir)) {
        	    // Duyệt qua tất cả các file trong thư mục
        	    for (const auto& entry : std::filesystem::directory_iterator(targetDir)) {
            	    // Nếu đuôi file là .zip thì lụm luôn
            		if (entry.path().extension() == ".zip") {
                	    zipPath = entry.path().string();
                	    break; // Chỉ lấy 1 file duy nhất rồi thoát vòng lặp
            		}
        	    }
    		} else {
        	    std::cout << "[LỖI] Thư mục '" << targetDir << "' không tồn tại!\n";
        	    break;
    		}

		// Nếu quét xong mà không thấy file zip nào
    		if (zipPath.empty()) {
        	    std::cout << "[LỖI] Không tìm thấy file .zip nào trong thư mục 'Firmware/'!\n";
        	    break;
    		}

    		std::cout << "[Main] Đã tự động chọn Firmware: " << zipPath << "\n";

                fotaApp.startRequestDownloadProcess(zipPath);
                break;
            }

	    case 't':
	    case 'T':
		if (!fotaApp.isProcessing()) {
                    std::cout << "[Main] Bắt đầu truyền dữ liệu (0x36 Transfer Data)...\n";
                    // Gọi hàm xử lý 0x36 bên trong FotaManager
                    fotaApp.startTransferDataProcess();
                } else {
                    std::cout << "[Main] System is busy! Please wait.\n";
                }
                break;

	    case 'e':
            case 'E':
                if (!fotaApp.isProcessing()) {
                    std::cout << "[Main] Requesting Transfer Exit (0x37)...\n";
                    fotaApp.startTransferExitProcess();
                } else {
                    std::cout << "[Main] System is busy! Please wait until the current task finishes.\n";
                }
                break;

            case 'q':
            case 'Q':
                std::cout << "[Main] Quitting...\n";
		fotaApp.stop(); // Đảm bảo dừng thread trước khi thoát
                g_keepRunning = false;
                break;

            default:
                std::cout << "[Main] Unknown command.\n";
                break;
        }

        // Chờ một chút để thread kia kịp in log ra màn hình cho đẹp
        // (Tránh log của thread và menu bị trộn lẫn)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 5. Kết thúc
    fotaApp.stop(); // Đảm bảo thread dừng hẳn
    std::cout << "[Main] Application Exited Successfully.\n";
    return 0;
}
