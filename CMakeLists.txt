cmake_minimum_required (VERSION 2.6)
project (horst)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

IF (CMAKE_COMPILER_IS_GNUCC)
  SET (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Wextra")
ENDIF (CMAKE_COMPILER_IS_GNUCC)

include_directories ("${PROJECT_SOURCE_DIR}")

set(SOURCES
    average.c 
    channel.c 
    conf_options.c
    control.c
    display-channel.c
    display-essid.c
    display-filter.c
    display-help.c
    display-history.c
    display-main.c
    display-spectrum.c
    display-statistics.c
    display.c
    essid.c
    ieee80211_util.c
    listsort.c
    main.c
    network.c
    node.c
    protocol_parser.c
    protocol_parser_wlan.c
    radiotap/radiotap.c
    util.c
    wlan_util.c
)

set(DEPENDS
   -lncurses
   -lm
)

if (NOT DEFINED DO_DEBUG)
    set(DO_DEBUG 0)
endif()


add_definitions(-DDO_DEBUG=${DO_DEBUG})

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_definitions(-DPCAP)
    add_definitions(-D__APPLE__)
    list(APPEND SOURCES capture-pcap.c)
    list(APPEND SOURCES ifctrl-dummy.c)
    list(APPEND DEPENDS -lpcap)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    if (NOT DEFINED WEXT)
        set(WEXT 0)
    endif()

    if (NOT DEFINED LIBNL)
        set(LIBNL "3.0")
    endif()

    if (NOT DEFINED PCAP)
        set(PCAP 0)
    endif()

    if (${WEXT} EQUAL 1)
        list(APPEND SOURCES ifctrl-wext.c)
    else()
        find_package(Libnl)
        include_directories( ${LIBNL_INCLUDE_DIR} )
        list(APPEND SOURCES ifctrl-nl80211.c)
        if (${LIBNL} MATCHES "tiny")
            list(APPEND DEPENDS -lnl-tiny)
        else()
            list(APPEND DEPENDS -lnl-3 -lnl-genl-3)
        endif()
    endif()

    if (${PCAP} EQUAL 0)
        list(APPEND SOURCES capture.c)
    else()
        add_definitions(-DPCAP)
        list(APPEND SOURCES capture-pcap.c)
        list(APPEND DEPENDS -lpcap)
    endif()

    list(APPEND SOURCES ifctrl-ioctl.c)
endif()

add_executable(horst ${SOURCES})
target_link_libraries(horst ${DEPENDS})
