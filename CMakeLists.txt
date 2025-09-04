cmake_minimum_required(VERSION 3.20)
project(CircuNet)

# استاندارد C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- مسیر نصب SDL2 و کتابخانه‌ها ---
set(SDL2_PATH "C:/MinGW/MinGW")

# include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include             # هدرهای خودت
    ${CMAKE_SOURCE_DIR}/cereal/include      # cereal
    ${SDL2_PATH}/include                    # کل پوشه include
)

# library directories
link_directories(
    ${SDL2_PATH}/lib
)

# --- فایل‌های پروژه ---
add_executable(CircuNet
    ${CMAKE_SOURCE_DIR}/src/main.cpp        # فایل اصلی توی src
    ${CMAKE_SOURCE_DIR}/src/app.rc          # resource ویندوز
)

# --- لینک کردن کتابخانه‌ها (ترتیب مهم) ---
target_link_libraries(CircuNet
    mingw32
    SDL2main
    SDL2
    SDL2_ttf
    ws2_32
)

# --- Resource ها (کپی assets و data کنار exe) ---
set(ASSETS_DIR ${CMAKE_SOURCE_DIR}/assets)
set(DATA_DIR   ${CMAKE_SOURCE_DIR}/data)

# کپی پوشه assets
add_custom_command(TARGET CircuNet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${ASSETS_DIR}
        $<TARGET_FILE_DIR:CircuNet>/assets
)

# کپی پوشه data
add_custom_command(TARGET CircuNet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${DATA_DIR}
        $<TARGET_FILE_DIR:CircuNet>/data
)
