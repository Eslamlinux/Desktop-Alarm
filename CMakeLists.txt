cmake_minimum_required(VERSION 3.10)
project(DesktopAlarm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add wxWidgets components including graphics
find_package(wxWidgets REQUIRED COMPONENTS core base adv aui xrc html net core xml propgrid)
include(${wxWidgets_USE_FILE})

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(${PROJECT_NAME} 
    main.cpp
)

target_link_libraries(${PROJECT_NAME} 
    ${wxWidgets_LIBRARIES}
    sqlite3
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Copy resources to build directory
file(COPY ${CMAKE_SOURCE_DIR}/sounds DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/locale DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/icons DESTINATION ${CMAKE_BINARY_DIR})

# Add compiler definitions for security features
target_compile_definitions(${PROJECT_NAME} PRIVATE
    ENABLE_ENCRYPTION
    SECURE_STORAGE
)
