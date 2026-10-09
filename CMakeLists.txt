cmake_minimum_required(VERSION 3.16)
project(onenode)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVAHI REQUIRED avahi-client)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Gui)
add_executable(onenode
    src/main.cpp
    src/mainwindow.cpp
    src/mdnsadvertiser.cpp
    src/pairingserver.cpp
    src/filetransfer.cpp
    resources/resources.qrc
)
target_include_directories(onenode PRIVATE src ${AVAHI_INCLUDE_DIRS})
target_link_libraries(onenode Qt6::Core Qt6::Widgets Qt6::Network Qt6::Gui ${AVAHI_LIBRARIES})

find_package(Qt6 COMPONENTS Test REQUIRED)
add_executable(onenode_test src/test_hmac.cpp)
target_include_directories(onenode_test PRIVATE src)
target_link_libraries(onenode_test Qt6::Core Qt6::Network Qt6::Test)