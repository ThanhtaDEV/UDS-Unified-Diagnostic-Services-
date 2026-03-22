#include "UdsException.h"
#include "UdsConstants.h"
#include <sstream>
#include <iomanip>

UdsException::UdsException(uint8_t nrcCode) : nrc(nrcCode) {
    // Tạo thông báo lỗi đầy đủ
    std::stringstream ss;
    ss << "UDS Error [NRC: 0x"
       << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)nrc
       << "] - " << getNrcDescription(nrc);
    message = ss.str();
}

const char* UdsException::what() const noexcept {
    return message.c_str();
}

uint8_t UdsException::getNRC() const {
    return nrc;
}

// TỪ ĐIỂN ÁNH XẠ MÃ LỖI (Dùng Enum thay vì số cứng)
std::string UdsException::getNrcDescription(uint8_t nrcCode) {
    // Ép kiểu sang Enum để code dễ đọc và match với file Constants
    switch (static_cast<Uds::Nrc>(nrcCode)) {
        case Uds::Nrc::GeneralReject:           return "General Reject";
        case Uds::Nrc::ServiceNotSupported:     return "Service Not Supported";
        case Uds::Nrc::SubFunctionNotSupported: return "Sub-function Not Supported";
        case Uds::Nrc::InvalidFormat:           return "Incorrect Message Length Or Invalid Format";
        case Uds::Nrc::ResponseTooLong:         return "Response Too Long";
        case Uds::Nrc::BusyRepeatRequest:       return "Busy Repeat Request";
        case Uds::Nrc::ConditionsNotCorrect:    return "Conditions Not Correct";
        case Uds::Nrc::RequestSequenceError:    return "Request Sequence Error";
        case Uds::Nrc::RequestOutOfRange:       return "Request Out Of Range";
        case Uds::Nrc::SecurityAccessDenied:    return "Security Access Denied";
        case Uds::Nrc::InvalidKey:              return "Invalid Key";
        case Uds::Nrc::ExceedNumberOfAttempts:  return "Exceed Number Of Attempts";

        case Uds::Nrc::UploadDownloadNotAccepted: return "Upload/Download Not Accepted";
        case Uds::Nrc::TransferDataSuspended:     return "Transfer Data Suspended";

        case Uds::Nrc::GeneralProgrammingFailure: return "General Programming Failure";
        case Uds::Nrc::WrongBlockSequenceCounter: return "Wrong Block Sequence Counter";
        case Uds::Nrc::ResponsePending:           return "Response Pending (Wait...)";

        case Uds::Nrc::SubFunctionNotSupportedInActiveSession: return "Sub-function Not Supported In Active Session";
        case Uds::Nrc::ServiceNotSupportedInActiveSession:     return "Service Not Supported In Active Session";

        // Các mã lỗi nội bộ Client
        case Uds::Nrc::ClientTimeout: return "Client Timeout (No Response from ECU)";
        case Uds::Nrc::ProtocolError: return "Protocol Error (Wrong SID/Format)";

        default: return "Unknown NRC";
    }
}
