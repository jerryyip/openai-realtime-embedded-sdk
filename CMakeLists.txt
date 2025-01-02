cmake_minimum_required(VERSION 3.19)

# add_compile_definitions(OPENAI_API_KEY="$ENV{OPENAI_API_KEY}")
# add_compile_definitions(WIFI_SSID="$ENV{WIFI_SSID}")
# add_compile_definitions(WIFI_PASSWORD="$ENV{WIFI_PASSWORD}")

add_compile_definitions(OPENAI_REALTIMEAPI="https://api.openai.com/v1/realtime?model=gpt-4o-mini-realtime-preview-2024-12-17")

set(EXTRA_COMPONENT_DIRS "src" "components/srtp" "components/peer" "components/esp-libopus")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_options(-fdiagnostics-color=always -Wno-error=format= -Wno-format)
project(openai-realtime)
