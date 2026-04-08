#include "UdsClient.h"
#include "UdsConstants.h"

#include <iostream>
#include <iomanip> // Để in hex đẹp hơn nếu cần debug

// Constructor & Dependency Injection
// Nhận vào Interface Transport để sau này dễ dàng thay thế (Mock -> SocketCAN)
UdsClient::UdsClient(ITransport* trans, SecurityManager* sec, int timeoutMs)
    : transport(trans), security(sec), p2_timeout(timeoutMs)
{
    if (!this->transport) {
        std::cerr << "[UDS_Client - Error] UdsClient initialized with NULL transport!\n";
    }
}

UdsClient::~UdsClient() {
    // Không delete transport vì nó được quản lý bởi bên ngoài (main)
}

// Helper Functions
void UdsClient::append32BitToVector(std::vector<uint8_t>& vec, uint32_t value) {
    vec.push_back((value >> 24) & 0xFF);
    vec.push_back((value >> 16) & 0xFF);
    vec.push_back((value >> 8)  & 0xFF);
    vec.push_back(value & 0xFF);
}

// Hàm cốt lõi (Core Logic)
// Chịu trách nhiệm điều phối: Gửi -> Chờ -> Check Lỗi -> Trả kết quả
UdsResponse UdsClient::sendAndWait(const UdsMessage& req) {
    // 1. Kiểm tra an toàn
    if (!transport) {
        throw UdsException(UdsClient::ERR_PROTOCOL); // Lỗi 0xFE (tự quy định)
    }

    // 2. Gửi lệnh đi (Sử dụng ITransport đã cập nhật nhận UdsMessage)
    transport->send(req);
    int current_timeout = p2_timeout; // Khởi tạo với timeout mặc định

    // 3. Chờ phản hồi (Chặn NRC 0x78)
    while(true) {
	// Tạo buffer rỗng để nhận dữ liệu
    	std::vector<uint8_t> rxBuffer;

	// 4. Chờ nhận dữ liệu vả xử lí timeout
    	// Gọi hàm receive của Transport (trả về bool: true=có data, false=timeout)
    	bool success = transport->receive(rxBuffer, current_timeout);
    	if (!success) {
            throw UdsException(UdsClient::ERR_TIMEOUT); // Lỗi 0xFF
        }

    	// 5. Phân tích gói tin nhận được
    	// Tận dụng class UdsResponse
    	// Nó sẽ tự động tách SID, tự kiểm tra xem có phải 0x7F hay không.
    	UdsResponse response(rxBuffer);

	// 6. CƠ CHẾ BẮT NRC 0x78 (RESPONSE PENDING)
        if (!response.isPositive() && response.getNRC() == static_cast<uint8_t>(Uds::Nrc::ResponsePending)) {
            std::cout << "[UDS_Client - Send&Wait] ECU busy (NRC 0x78). Extending timeout...\n";
            current_timeout = 5000; // Gia hạn thêm 5 giây chờ ECU xóa Flash
            continue;               // Bỏ qua các bước dưới, quay lại vòng lặp chờ tiếp
        }

    	// 7. Kiểm tra Logic UDS (Positive vs Negative)
    	if (!response.isPositive()) {
        // Nếu là Negative Response (0x7F...), lấy mã NRC ra và ném Exception.
        // UdsException sẽ tự tra từ điển để in ra dòng thông báo lỗi (VD: "Invalid Key")
            throw UdsException(response.getNRC());
    	}

    	// 8. Kiểm tra SID khớp lệnh
    	uint8_t expectedSid = Uds::getPositiveResponseSid(static_cast<Uds::Sid>(req.getSid()));
    	if (response.getSid() != expectedSid) {
            std::cerr << "[UDS_Client - Send&Wait] <ERROR> Protocol Mismatch! Req: 0x" << std::hex << (int)req.getSid()
                      << " -> Exp: 0x" << (int)expectedSid
                      << ", Got: 0x" << (int)response.getSid() << std::dec << std::endl;

            // Ném lỗi giao thức (Protocol Error)
            throw UdsException(UdsClient::ERR_PROTOCOL);
        }

    	//8. mọi thứ ok thì trả về
    	return response;
    }
}

// Các dịch vụ cơ bản (API cho Application)

// Service 0x10: Diagnostic Session Control
UdsResponse UdsClient::requestSession(uint8_t sessionType) {
    std::cout << "[UDS_Client - 0x10] Calling API: requestSession(0x0"
              << std::hex << (int)sessionType << ")" << std::endl;

    // Tạo payload chứa tham số (Sub-function)
    std::vector<uint8_t> payload = {sessionType};

    // Đóng gói thành UdsMessage (SID 0x10)
    UdsMessage req(static_cast<uint8_t>(Uds::Sid::DiagnosticSessionControl), payload);

    // Gửi và chờ phản hồi
    UdsResponse resp = sendAndWait(req);

    // Xử lý Logic Timing từ VCU
    if(resp.isPositive()){
	std::vector<uint8_t> data = resp.getData();
	// Cấu trúc data trả về từ VCU
	// [0]: SessionType
	// [1]: P2_High, [2]: P2_Low
        // [3]: P2*_High, [4]: P2*_Low
	if(data.size() >= 5){
	    // Lấy thông số P2* (P2 Star) - Thời gian chờ tối đa khi VCU bận
            uint16_t p2_star_val = (data[3] << 8) | data[4];

            // In ra để debug
            std::cout << "[UDS_Client - 0x10] Session Accepted. VCU Params:" << std::endl;
            std::cout << "[UDS_Client - 0x10]  - P2 (Max Response Time): " << ((data[1] << 8) | data[2]) << "ms" << std::endl;
            std::cout << "[UDS_Client - 0x10]  - P2* (Max Busy Time): " << p2_star_val << " (unit 10ms)" << std::endl;
	    // CẬP NHẬT TIMEOUT CHO CLIENT
            // Code VCU bạn gửi: 0x01F4 (500) -> chú thích là 5000ms => đơn vị là 10ms
            if (p2_star_val > 0) {
                this->p2_timeout = p2_star_val * 10;
                std::cout << "[UDS_Client - 0x10] UPDATED TIMEOUT to " << this->p2_timeout << "ms <<" << std::endl;
            }
        } else {
            std::cout << "[UDS_Client - 0x10] <WARNING> VCU response too short, keeping default timeout." << std::endl;
        }
    } else {
        std::cerr << "[UDS_Client - 0x10] Session Request Failed! NRC: 0x"
                  << std::hex << (int)resp.getNRC() << std::dec << std::endl;
    }

    return resp;
}

// Service 0x3E: Tester Present
void UdsClient::sendTesterPresent() {
    // 1. Tạo Payload: 0x80 (Suppress Positive Response)
    // "Đừng trả lời nếu thành công"
    std::vector<uint8_t> payload = { 0x80 };
    // 2. Đóng gói thành UdsMessage
    //    Dùng static_cast để ép kiểu Enum class sang uint8_t
    UdsMessage req(static_cast<uint8_t>(Uds::Sid::TesterPresent), payload);
    // 3. Gửi đi và KHÔNG CHỜ (Fire & Forget)
    if (transport) {
        transport->send(req);
        std::cout << "[UDS_Client - 0x3E] <HEARTBEAT> Sent 3E 80" << std::endl;
    }
}

// Service 0x11: ECU Reset
UdsResponse UdsClient::requestHardReset() {
    std::cout << "[UDS_Client - 0x11] Calling API: requestHardReset()" << std::endl;

    // Sub-function 0x01 = Hard Reset
    std::vector<uint8_t> payload = {0x01};

    // Đóng gói thành UdsMessage (SID 0x11)
    UdsMessage req(static_cast<uint8_t>(Uds::Sid::EcuReset), payload);

    return sendAndWait(req);
}

// Service 0x27: Security Access
bool UdsClient::unlockSecurity(uint8_t level) {
    // Level 1: 0x01 (Request Seed)
    std::cout << "[UDS_Client - 0x27] Starting Security Access (Level 0x" << std::hex << (int)level << ")..." << std::endl;
    try {
	// --- BƯỚC A: REQUEST SEED (0x27 01) ---
	std::vector<uint8_t> payloadSeed = { level };
	UdsMessage reqSeed(static_cast<uint8_t>(Uds::Sid::SecurityAccess), payloadSeed);
	UdsResponse respSeed = sendAndWait(reqSeed);
	// Kiểm tra phản hồi
	if (!respSeed.isPositive()) {
            std::cerr << "[UDS_Client - 0x27] Request Seed Failed! NRC: 0x"
                      << std::hex << (int)respSeed.getNRC() << std::endl;
            // Xử lý NRC đặc biệt của VCU
            if (respSeed.getNRC() == 0x37) {
		std::cerr << "[UDS_Client - 0x27] <WARNING>: System is in Time Delay penalty. Wait 10s!" << std::endl;
	    }
            return false;
	}

	// --- BƯỚC B: GET SEED & CHECK LOCKED STATUS ---
	const std::vector<uint8_t>& seedData = respSeed.getData();

	// Kiểm tra độ dài: 1 byte SID + 1 byte subfunc + 4 byte seed
	if (seedData.size() < 5) {
	    std::cout << "[UDS_Client - 0x27] Seed response too short!" << std::endl;
	    return false;
	}

	// Parse Seed (Big Endian)
	uint32_t seed = 0;
	seed |= (uint32_t)seedData[1] << 24;
	seed |= (uint32_t)seedData[2] << 16;
	seed |= (uint32_t)seedData[3] << 8;
	seed |= (uint32_t)seedData[4];

	std::cout << "[Client - 0x27] Got Seed: 0x" << std::hex << seed << std::endl;

	// [LOGIC VCU]: Nếu Seed = 0 -> Đã Unlock rồi -> Return True
	if (seed == 0x00000000) {
	    std::cout << "[Client - 0x27] VCU says: Already Unlocked! (Seed is 0)" << std::endl;
            return true;
        }

        // --- BƯỚC C: COMPUTE KEY ---
        // Thuật toán: Key = Seed ^ Mask (Lấy Mask từ UdsConstants)
        uint32_t key = seed ^ Uds::Security::SECURITY_MASK;
        std::cout << "[UDS_Client - 0x27] Computed Key: 0x" << std::hex << key
	          << "(Mask: 0x" << Uds::Security::SECURITY_MASK << ")" << std::endl;

        // --- BƯỚC D: SEND KEY (0x27 02) ---
        std::vector<uint8_t> payloadKey;
        payloadKey.push_back(level + 1); // Sub-function: Send Key (0x02)

        // Đóng gói Key 4 bytes (Big Endian)
        payloadKey.push_back((key >> 24) & 0xFF);
        payloadKey.push_back((key >> 16) & 0xFF);
        payloadKey.push_back((key >> 8)  & 0xFF);
        payloadKey.push_back((key)       & 0xFF);

        UdsMessage reqKey(static_cast<uint8_t>(Uds::Sid::SecurityAccess), payloadKey);
        UdsResponse respKey = sendAndWait(reqKey);

        // --- BƯỚC E: VERIFY RESULT ---
        if (respKey.isPositive()) {
            std::cout << "[UDS_Client - 0x27] Security Access UNLOCKED successfully!" << std::endl;
            return true;
        }

        else {
	    // Lấy mã lỗi
            uint8_t nrc = respKey.getNRC();

	    std::cerr << "[UDS_Client - 0x27] Unlock Failed! NRC: 0x"
                      << std::hex << (int)respKey.getNRC() << std::endl;
	    switch (static_cast<Uds::Nrc>(nrc)) {
                case Uds::Nrc::SecurityAccessDenied: // 0x33
                    std::cerr << "[UDS_Client - 0x27] <ERROR> NRC - 0x33: Security Access Denied.\n"
                              << "Reason: Wrong Sequence? (Did you Request Seed first?)\n";
                    break;

                case Uds::Nrc::InvalidKey: // 0x35
                    std::cerr << "[UDS_Client - 0x27] <ERROR> NRC - 0x35: Invalid Key.\n"
                              << "Reason: Key mismatch. Check your MASK (0x12345678) or Algorithm.\n";
                    break;

                case Uds::Nrc::ExceedNumberOfAttempts: // 0x36
                    std::cerr << "[UDS_Client - 0x27] <FATAL> NRC - 0x36: Exceeded Number of Attempts!\n"
                              << "Action: VCU has blocked access. Wait 10 seconds or Cycle Power.\n";
                    break;

                case Uds::Nrc::RequiredTimeDelayNotExpired: // 0x37
                    std::cerr << "[UDS_Client - 0x27] <FATAL> NRC - 0x37: Time Delay Not Expired.\n"
                              << "Action: You are in penalty mode. Please wait...\n";
                    break;

                default:
                    std::cerr << "[UDS_Clident - 0x27] <ERROR> Unknown NRC. Check ISO-14229 spec.\n";
                    break;
            }
            return false;
        }
     }

     catch (const std::exception& e) {
            // Bắt lỗi văng app (crash) từ hàm sendAndWait
            std::cerr << "[UDS_Client - 0x27] Exception caught during Security Access: " << e.what() << "\n";
            return false;
     }
}

// Service 0x31: Routine Control (Erase Memory)
/*UdsResponse UdsClient::routineControl(uint16_t routineId, uint8_t subFunc, const std::vector<uint8_t>& optionRecord) {
    std::cout << "\n[Client] -> Calling API: routineControl(RID: 0x" << std::hex << routineId << ")" << std::endl;

    std::vector<uint8_t> payload;
    payload.push_back(subFunc);               // Sub-function (e.g. 0x01 Start)
    payload.push_back((routineId >> 8) & 0xFF); // RID High
    payload.push_back(routineId & 0xFF);        // RID Low

    // Nối thêm Option Record (Address + Size)
    payload.insert(payload.end(), optionRecord.begin(), optionRecord.end());

    UdsMessage req(static_cast<uint8_t>(Uds::Sid::RoutineControl), payload);

    // Lưu ý: Lệnh xóa Flash có thể lâu, nếu cần timeout dài hơn thì set tạm thời
    // transport->setTimeout(5000);
    return sendAndWait(req);
}*/

// Service 0x34: Request Download
uint32_t UdsClient::requestDownload(uint32_t address, uint32_t size) {
    std::cout << "[UDS_Client - 0x34] Calling API: requestDownload(Addr: 0x" << std::hex << address
    	      << ", Size: " << std::dec << size << "bytes)\n";

    std::vector<uint8_t> payload;
    // 1. Đóng gói DFI và ALFID
    payload.push_back(Uds::DFI_RAW_DATA); // DataFormatIdentifier (0x00 = No compression/encryption)
    payload.push_back(Uds::ALFID_4B_SIZE_4B_ADDR); // AddressAndLengthFormatIdentifier (4 byte Addr, 4 byte Size)

    // 2. Đóng gói Address và Size (4 bytes mỗi tham số)
    append32BitToVector(payload, address);
    append32BitToVector(payload, size);

    // 3. Khởi tạo Message với SID 0x34
    UdsMessage reqMsg(static_cast<uint8_t>(Uds::Sid::RequestDownload), payload);

    try {
	// 4. Gửi và chờ phản hồi qua "Trái tim" sendAndWait
        // Không cần quan tâm 0x78 ở đây, sendAndWait đã tự lo!
        UdsResponse resp = sendAndWait(reqMsg);

        // 5. Đến đây, chắc chắn 100% là Positive Response (0x74)
        std::vector<uint8_t> respData = resp.getData();

        // respData[0] là LengthFormatIdentifier (VD: 0x20)
        // respData[1] và respData[2] là MaxNumberOfBlockLength (MaxBlockLength)
        if (respData.size() >= 3) {
            uint32_t maxBlockLen = (respData[1] << 8) | respData[2];
            std::cout << "[UDS_Client - 0x34] SUCCESS! Max Block Length: " << maxBlockLen << " bytes.\n";
            return maxBlockLen;
        } else {
            std::cerr << "[UDS_Client - 0x34] <ERROR> Invalid positive response payload length!\n";
            return 0; // Trả về 0 nghĩa là FOTA thất bại
        }
    }
    catch (const UdsException& e) {
	// Bắt mọi lỗi từ Timeout, Giao thức, đến NRC (0x13, 0x22, 0x31...)
        std::cerr << "[UDS_Client - 0x34] <ERROR> RequestDownload Failed: " << e.what() << std::endl;
        return 0;
    }
}

// Service 0x36: Transfer Data
bool UdsClient::transferData(uint8_t blockSequenceCounter, const std::vector<uint8_t>& dataChunk) {
    std::cout << "[UDS_Client - 0x36] TransferData (Block: 0x"
	      << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)blockSequenceCounter
	      << std::dec << ", Size: " << dataChunk.size() << "bytes)\n";
    std::vector<uint8_t> payload;
    payload.push_back(blockSequenceCounter); // 1 byte Block Sequence Counter nằm đầu tiên
    payload.insert(payload.end(), dataChunk.begin(), dataChunk.end());	// Nối toàn bộ mảng dataChunk (Firmware) vào phía sau Block

    // Gắn "Tem" và tạo Message
    UdsMessage reqMsg(static_cast<uint8_t>(Uds::Sid::TransferData), payload);

    // Gửi và nhận phản hồi
    try {
	// Gửi lệnh đi và chờ nhận phản hồi
        UdsResponse resp = sendAndWait(reqMsg);
	// Kiểm tra tính hợp lệ của Response từ VCU
	std::vector<uint8_t> respData = resp.getData();

        // RespData[0] chính là cái Block Counter mà VCU phản hồi lại
        if (!respData.empty() && respData[0] == blockSequenceCounter) {
	    // Khớp hoàn toàn! Ghi Flash thành công
            return true;
        } else {
            std::cerr << "[UDS_Client - 0x36] <ERROR> VCU responded with mismatched Block Counter!\n" << std::endl;
            return false;
        }
    } catch (const UdsException& e) {
        std::cerr << "[UDS_Client - 0x36] <ERROR> TransferData Block " << (int)blockSequenceCounter << " Failed: " << e.what() << std::endl;
        return false;
    }
}

// Service 0x37: Request Transfer Exit
bool UdsClient::requestTransferExit() {
    std::cout << "[UDS_Client - 0x37] Calling API: requestTransferExit()" << std::endl;
    UdsMessage req(static_cast<uint8_t>(Uds::Sid::RequestTransferExit));

    try {
        sendAndWait(req);
        return true;
    } catch (const UdsException& e) {
        std::cerr << "[UDS_Client - 0x37] <ERROR> RequestTransferExit Failed: " << e.what() << std::endl;
        return false;
    }
}
