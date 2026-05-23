# Yêu cầu phiên bản CMake tối thiểu
cmake_minimum_required(VERSION 3.10)

# Tên dự án
project(AutomotiveGateway VERSION 1.0.1)

# Hệ thống sử dụng chuẩn C++17 (tương đương -std=c++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# TÌM KIẾM CÁC THƯ VIỆN HỆ THỐNG
# 1. Threading (-pthread)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# 2. CURL (-lcurl)
find_package(CURL REQUIRED)

# 3. OpenSSL (-lssl -lcrypto)
find_package(OpenSSL REQUIRED)

# 4. Zlib (-lz)
find_package(ZLIB REQUIRED)

# DANH SÁCH MÃ NGUỒN
# Liệt kê toàn bộ file .cpp
set(SOURCES
    Master.cpp
    FOTA_Manager.cpp
    RemoteDiag_Manager.cpp
    UdsClient.cpp
    UdsMessage.cpp
    UdsResponse.cpp
    UdsException.cpp
    SocketCanTransport.cpp
)

# TẠO FILE THỰC THI VÀ LIÊN KẾT
# Tạo file thực thi tên là "Master" từ các file mã nguồn
set(APP_NAME "Master_v${PROJECT_VERSION}")

add_executable(${APP_NAME} ${SOURCES})

# Chỉ định thư mục chứa các file Header (.h)
target_include_directories(${APP_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Liên kết (Link) các thư viện đã tìm thấy vào file thực thi "Master"
target_link_libraries(${APP_NAME} PRIVATE
    Threads::Threads
    ${CURL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
)
