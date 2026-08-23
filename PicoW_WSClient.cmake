# Add library cpp files

if (NOT DEFINED PICOW_WSCLIENT_PATH)
    set(PICOW_WSCLIENT_PATH "${CMAKE_CURRENT_LIST_DIR}")
endif()


add_library(picow_wsclient STATIC)

target_sources(picow_wsclient PUBLIC
	${PICOW_WSCLIENT_PATH}/src/Request.cpp
	${PICOW_WSCLIENT_PATH}/src/RequestObserver.cpp
	${PICOW_WSCLIENT_PATH}/src/WSTCPTransport.cpp
	${PICOW_WSCLIENT_PATH}/src/WSTLSTransBlock.cpp
	${PICOW_WSCLIENT_PATH}/src/WSTransport.cpp
	${PICOW_WSCLIENT_PATH}/src/WifiHelper.cpp
)


# Add include directory
target_include_directories(picow_wsclient PUBLIC 
    ${PICOW_WSCLIENT_PATH}/src
)

target_link_libraries(picow_wsclient PUBLIC 
    pico_stdlib
    pico_aon_timer
    pico_lwip_sntp
    FreeRTOS-Kernel
    LWIP_PORT
    pico_cyw43_arch_lwip_sys_freertos
    coreHTTP
    urilib
    wolfssl
    json_maker
    )



target_compile_definitions(picow_wsclient PRIVATE
	LIBRARY_LOG_LEVEL=LOG_ERROR
	LIBRARY_LOG_NAME="PICOW_WSCLIENT"
)
