#include "RemoteDiag_Manager.h"
#include <chrono>

// Khởi tạo và quản lý dòng đời luồng
RemoteDiag_Manager::RemoteDiag_Manager(UdsClient* pClient, int intervalMs)
    : m_pUdsClient(pClient),
      m_isRunning(false),
      m_pollingIntervalMs(intervalMs)
{
}

RemoteDiag_Manager::~RemoteDiag_Manager() {
    stop();
}

void RemoteDiag_Manager::start() {
    if (!m_isRunning) {
        m_isRunning = true;
        // Khởi tạo thread và trỏ nó vào hàm run() của object hiện tại
        m_workerThread = std::thread(&RemoteDiag_Manager::run, this);
        std::cout << "[RemoteDiag_Manager] Manager Started.\n";
    }
}

void RemoteDiag_Manager::stop() {
    if (m_isRunning) {
        m_isRunning = false;
        if (m_workerThread.joinable()) {
            m_workerThread.join(); // Đợi thread làm xong việc hiện tại rồi mới đóng
        }
        std::cout << "[RemoteDiag_Manager] Manager Stopped.\n";
    }
}

// Worker Thread
void RemoteDiag_Manager::run(){
    while (m_isRunning) {
	// Gọi hàm đi lấy mã lỗi
	pollActiveDTCs();

	int elapsedMs = 0;
	while (m_isRunning && (elapsedMs < m_pollingIntervalMs)) {
	    // Sleep cho đến chu kỳ tiếp theo
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
	    elapsedMs += 100;
	}
    }
}


// LOGIC: Lấy và Phân tích lỗi
void RemoteDiag_Manager::pollActiveDTCs(){
    try {
        std::cout << "[RemoteDiag_Manager] Polling for active DTCs...\n";

        // 1. Call Middleware. Nếu FOTA đang khóa bus, hàm này sẽ bị "đóng băng" ở đây.
        // Khi FOTA done, auto run.
        UdsResponse response = m_pUdsClient->readDTCList(Uds::ReadDTC::StatusMask_Active);

        // 2. Lấy toàn bộ mảng byte trả về (Payload)
        std::vector<uint8_t> payload = response.getData();

        // 3. Đẩy mảng byte đi bóc tách
        parseDTCPayload(payload);

    } catch (const UdsException& e) {
        // Nếu Timeout hoặc lỗi mạng, pick ở đây để không làm sập (crash) toàn bộ chương trình
        std::cerr << "[RemoteDiag_Manager] Failed to poll DTCs. Error Code: 0x"
                  << std::hex << (int)e.getNRC() << std::dec << "\n";
    }
}

// LOGIC: Bóc tách mảng byte (Parsing Logic)
void RemoteDiag_Manager::parseDTCPayload(const std::vector<uint8_t>& payload) {
    // KIẾN TRÚC PAYLOAD CỦA UDS 0x19 0x02: (SID Uds_Response đã xử lý)
    // Index 0: 0x02 (Sub-Func)
    // Index 1: 0xFF (Status Availability Mask)
    // Index 2,3,4: DTC 1 (High, Mid, Low)
    // Index 5: Status 1
    // Index 6,7,8: DTC 2
    // Index 9: Status 2

    // Kiểm tra kích thước tối thiểu
    if (payload.size() <= 2) {
        std::cout << "[RemoteDiag_Manager] No DTCs found. System is clean.\n";
	// Nếu xe báo 0 lỗi, locked và clear mảng -> Báo Web 0 lỗi
	std::lock_guard<std::mutex> lock(m_mtxDemDataLock);
        m_activeDTCList.clear();
        return;
    }

    uint8_t expectedSubFunc = Uds::ReadDTC::ReportDTCByStatusMask;

    // Kiểm tra Sub-function
    if (payload[0] != expectedSubFunc) {
        std::cerr << "[RemoteDiag] Invalid payload format. "
		  << " Expected Sub-func 0x" << (int)expectedSubFunc
		  << " but got 0x " << (int)payload[0] << std::dec << ".\n";
        return;
    }

    // TỰ ĐỘNG KHÓA (Lock Guard)
    std::lock_guard<std::mutex> lock(m_mtxDemDataLock);
    // Xóa danh sách cũ đi để cập nhật danh sách mới
    m_activeDTCList.clear();

    // Bắt đầu duyệt từ Index 2 (Sub-func và Availability Mask)
    // Mỗi lỗi chiếm đúng 4 byte (3 byte mã + 1 byte status)
    for (size_t i = 2; i + 3 < payload.size(); i += 4) {
        DTCRecord record;

        // Dịch bit (Bitwise Shift) để gộp 3 byte rời rạc thành 1 số 32-bit duy nhất
        record.dtcCode = (payload[i] << 16) | (payload[i+1] << 8) | payload[i+2];

        // Lấy byte Status
        record.status = payload[i+3];

        // Lưu vào danh sách
        m_activeDTCList.push_back(record);

        std::cout << "[RemoteDiag_Manager] Found DTC: 0x" << std::hex << record.dtcCode
                  << " with Status: 0x" << (int)record.status << std::dec << "\n";
    }
}

std::vector<DTCRecord> RemoteDiag_Manager::getActiveDTCs() const {
    return m_activeDTCList;
}

bool RemoteDiag_Manager::Dem_GetActiveDTCList(std::vector<DTCRecord>& out_dtcList)
{
    // Locked để copy dữ liệu - Auto Unlock when done copy
    std::lock_guard<std::mutex> lock(m_mtxDemDataLock);
    out_dtcList = m_activeDTCList; // Copy vào "out_dtcList" của Web Poller
    return true;
}

