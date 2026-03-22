// File: UdsConstants.h
#pragma once
#include <cstdint>

namespace Uds {
    // List SID chuẩn ISO 14229 phù hợp FOTA
    enum class Sid : uint8_t {
	// Diagnostic & Communication Management
	DiagnosticSessionControl = 0x10,
	EcuReset                 = 0x11,
	SecurityAccess           = 0x27,
	TesterPresent            = 0x3E,

	// Remote Activation
        RoutineControl           = 0x31,

	// Upload/Download (FOTA Core)
        RequestDownload          = 0x34,
        TransferData             = 0x36,
        RequestTransferExit      = 0x37,

	// // Response IDs
        NegativeResponse         = 0x7F
    };

    // 2. Các tham số cấu hình (Parameters) cho FOTA
    // DFI: Data Format Identifier (Không nén, không mã hóa)
    constexpr uint8_t DFI_RAW_DATA = 0x00;

    // ALFID: Address and Length Format Identifier
    // 4 bit cao (Size = 4 bytes) | 4 bit thấp (Address = 4 bytes)
    constexpr uint8_t ALFID_4B_SIZE_4B_ADDR = 0x44;

    // List NRC chuẩn ISO 14229 + Mã lỗi nội bộ SoC
    enum class Nrc : uint8_t {
        // --- ISO 14229 Standard NRCs ---
        GeneralReject           		= 0x10,
        ServiceNotSupported     		= 0x11,
        SubFunctionNotSupported 		= 0x12,
        InvalidFormat           		= 0x13,	// Gửi sai ALFID hoặc thiếu byte
        ResponseTooLong         		= 0x14,
        BusyRepeatRequest       		= 0x21,
        ConditionsNotCorrect    		= 0x22,	// Quên chưa gửi lệnh vào Programming Session (0x10 0x02)
        RequestSequenceError    		= 0x24,	// Nhảy cóc (Ví dụ: Gửi 0x36 khi chưa pass 0x34)
        RequestOutOfRange       		= 0x31,	// File quá to (> vùng nhớ) hoặc sai địa chỉ Sector
        SecurityAccessDenied    		= 0x33, // Lỗi sai Key
        InvalidKey              		= 0x35, // Lỗi key không khớp
        ExceedNumberOfAttempts  		= 0x36, // Sai quá nhiều lần (bị Block)
	RequiredTimeDelayNotExpired 		= 0x37,	// Đang bị phạt chờ (Delay)
        UploadDownloadNotAccepted 		= 0x70, // VCU xóa Flash thất bại (Lỗi phần cứng)
        TransferDataSuspended   		= 0x71,	// Quá trình truyền bị đình chỉ
        GeneralProgrammingFailure 		= 0x72,	// VCU ghi Flash 0x36 thất bại
        WrongBlockSequenceCounter 		= 0x73,	// SoC đếm sai số thứ tự gói tin 0x36
        ResponsePending         		= 0x78, // VCU bận, yêu cầu chờ
        SubFunctionNotSupportedInActiveSession 	= 0x7E,
        ServiceNotSupportedInActiveSession     	= 0x7F,

        // Internal Client Errors (SoC defined)
        ClientTimeout           = 0xFF, // Timeout (No Response)
        ProtocolError           = 0xFE  // Wrong SID/Format parsing error
    };

    namespace Session {
	constexpr uint8_t Default       = 0x01;
        constexpr uint8_t Programming   = 0x02; // Dùng cho FOTA
        constexpr uint8_t Extended      = 0x03;
    }

    // Tham số cho Service 0x11 (ECU Reset)
    namespace Reset {
        constexpr uint8_t Hard          = 0x01; // Power On Reset
        constexpr uint8_t KeyOffOn      = 0x02;
        constexpr uint8_t Soft          = 0x03;
    }

    // Tham số cho Service 0x27 (Security Access)
    namespace Security {
        constexpr uint8_t RequestSeed   = 0x01; // Xin Seed (Level 1)
        constexpr uint8_t SendKey       = 0x02; // Gửi Key (Level 1)
	constexpr uint32_t SECURITY_MASK = 0x12345678; // Thuật toán Level 1
    }

    // Hàm tính SID phản hồi thành công (Request + 0x40)
    constexpr uint8_t getPositiveResponseSid(Sid requestSid) {
        return static_cast<uint8_t>(requestSid) + 0x40;
    }
}
