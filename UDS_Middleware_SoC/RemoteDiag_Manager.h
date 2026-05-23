#ifndef REMOTEDIAG_MANAGER_H
#define REMOTEDIAG_MANAGER_H

#include "UdsClient.h"
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <iostream>

struct DTCRecord {
    uint32_t dtcCode; // Mã lỗi 3 byte (Ví dụ: 0xB00316)
    uint8_t status;   // Trạng thái 1 byte (Ví dụ: 0x09 - Active)
};

class RemoteDiag_Manager{
private:
    // Trỏ tới cổng giao tiếp chung
    UdsClient* m_pUdsClient;

    // Quản lý luồng
    std::thread m_workerThread;
    std::atomic<bool> m_isRunning;
    int m_pollingIntervalMs;

    // Dữ liệu nghiệp vụ
    std::vector<DTCRecord> m_activeDTCList;

    // Mutex - bảo vệ danh sách lỗi (m_activeDTCList)
    std::mutex m_mtxDemDataLock;

    // Hàm nội bộ
    void run();
    void pollActiveDTCs();
    void parseDTCPayload(const std::vector<uint8_t>& payload);
public:
    RemoteDiag_Manager(UdsClient* pClient, int intervalMs = 300000); // 5 minutes
    ~RemoteDiag_Manager();

    void start();
    void stop();

    // API cho Web Dashboard lấy dữ liệu hiển thị
    std::vector<DTCRecord> getActiveDTCs() const;

    // Getter - Web Poller lấy mã lỗi trong "m_activeDTCList"
    bool Dem_GetActiveDTCList(std::vector<DTCRecord>& out_dtcList);

};

#endif // REMOTEDIAG_MANAGER_H
