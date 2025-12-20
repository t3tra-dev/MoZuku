cmake_minimum_required(VERSION 3.16)

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

find_package(PkgConfig REQUIRED)

find_program(MECAB_CONFIG mecab-config)

if(MECAB_CONFIG)
    execute_process(
        COMMAND ${MECAB_CONFIG} --libs-only-L
        OUTPUT_VARIABLE MECAB_LIBRARY_DIRS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${MECAB_CONFIG} --libs-only-l
        OUTPUT_VARIABLE MECAB_LINK_LIBRARIES
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${MECAB_CONFIG} --inc-dir
        OUTPUT_VARIABLE MECAB_INCLUDE_DIRS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    string(REPLACE "-L" "" MECAB_LIBRARY_DIRS "${MECAB_LIBRARY_DIRS}")
    string(REPLACE "-l" "" MECAB_LINK_LIBRARIES "${MECAB_LINK_LIBRARIES}")
    string(STRIP "${MECAB_LIBRARY_DIRS}" MECAB_LIBRARY_DIRS)
    string(STRIP "${MECAB_LINK_LIBRARIES}" MECAB_LINK_LIBRARIES)
    string(STRIP "${MECAB_INCLUDE_DIRS}" MECAB_INCLUDE_DIRS)

    separate_arguments(MECAB_LINK_LIBRARIES)

    set(MECAB_FOUND TRUE)
    message(STATUS "mecab-configによりMeCabを発見:")
    message(STATUS "  インクルードディレクトリ: ${MECAB_INCLUDE_DIRS}")
    message(STATUS "  ライブラリディレクトリ: ${MECAB_LIBRARY_DIRS}")
    message(STATUS "  リンクライブラリ: ${MECAB_LINK_LIBRARIES}")
else()
    message(STATUS "mecab-configが見つかりません。標準システムパスを試行中...")

    find_path(MECAB_INCLUDE_DIRS mecab.h
        HINTS ${MECAB_INCLUDE_HINT}
    )

    find_library(MECAB_LIBRARIES
        NAMES libmecab mecab
        HINTS ${MECAB_LIBRARY_HINT}
    )

    if(MECAB_INCLUDE_DIRS AND MECAB_LIBRARIES)
        set(MECAB_FOUND TRUE)
        message(STATUS "システムパスでMeCabを発見:")
        message(STATUS "  ヘッダー: ${MECAB_INCLUDE_DIRS}")
        message(STATUS "  ライブラリ: ${MECAB_LIBRARIES}")
    endif()
endif()

find_program(CABOCHA_CONFIG cabocha-config)

if(CABOCHA_CONFIG)
    execute_process(
        COMMAND ${CABOCHA_CONFIG} --cflags
        OUTPUT_VARIABLE CABOCHA_CFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${CABOCHA_CONFIG} --libs
        OUTPUT_VARIABLE CABOCHA_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    string(REGEX MATCHALL "-I[^ ]*" CABOCHA_INCLUDE_FLAGS "${CABOCHA_CFLAGS}")
    foreach(flag ${CABOCHA_INCLUDE_FLAGS})
        string(SUBSTRING ${flag} 2 -1 include_dir)
        list(APPEND CABOCHA_INCLUDE_DIRS ${include_dir})
    endforeach()

    if(CABOCHA_LIBS)
        separate_arguments(CABOCHA_LIB_FLAGS UNIX_COMMAND "${CABOCHA_LIBS}")
        foreach(flag ${CABOCHA_LIB_FLAGS})
            if(flag MATCHES "^-L(.+)")
                list(APPEND CABOCHA_LIBRARY_DIRS "${CMAKE_MATCH_1}")
            elseif(flag MATCHES "^-l(.+)")
                list(APPEND CABOCHA_LINK_LIBRARIES "${CMAKE_MATCH_1}")
            endif()
        endforeach()
    endif()

    if(CABOCHA_LINK_LIBRARIES)
        set(CABOCHA_FOUND TRUE)
        message(STATUS "cabocha-configによりCaboChaを発見:")
        message(STATUS "  インクルードディレクトリ: ${CABOCHA_INCLUDE_DIRS}")
        message(STATUS "  ライブラリ: ${CABOCHA_LINK_LIBRARIES}")
        message(STATUS "  ライブラリディレクトリ: ${CABOCHA_LIBRARY_DIRS}")
    endif()
endif()

if(NOT CABOCHA_FOUND)
    message(STATUS "cabocha-configが見つかりません。標準システムパスを試行中。")

    find_path(CABOCHA_INCLUDE_DIRS cabocha.h
        HINTS ${CABOCHA_INCLUDE_HINT}
    )

    find_library(CABOCHA_LIBRARIES
        NAMES libcabocha cabocha
        HINTS ${CABOCHA_LIBRARY_HINT}
    )

    if(CABOCHA_INCLUDE_DIRS AND CABOCHA_LIBRARIES)
        set(CABOCHA_LINK_LIBRARIES ${CABOCHA_LIBRARIES})
        set(CABOCHA_FOUND TRUE)
        message(STATUS "システムパスでCaboChaを発見:")
        message(STATUS "  ヘッダー: ${CABOCHA_INCLUDE_DIRS}")
        message(STATUS "  ライブラリ: ${CABOCHA_LIBRARIES}")
    else()
        message(FATAL_ERROR "CaboCha が見つかりません。cabocha.h または libcabocha が見つかりませんでした。")
    endif()
endif()

find_path(CRFPP_INCLUDE_DIRS crfpp.h
    HINTS ${CRFPP_INCLUDE_HINT}
    PATH_SUFFIXES crf++
)

find_library(CRFPP_LIBRARIES
    NAMES libcrfpp crfpp
    HINTS ${CRFPP_LIBRARY_HINT}
)

if(CRFPP_INCLUDE_DIRS AND CRFPP_LIBRARIES)
    set(CRFPP_FOUND TRUE)
    message(STATUS "手動検索によりCRF++を発見:")
    message(STATUS "  ヘッダー: ${CRFPP_INCLUDE_DIRS}")
    message(STATUS "  ライブラリ: ${CRFPP_LIBRARIES}")
else()
    message(STATUS "CRF++が見つかりません（オプション）")
endif()

set(MECAB_FOUND ${MECAB_FOUND} PARENT_SCOPE)
set(MECAB_INCLUDE_DIRS ${MECAB_INCLUDE_DIRS} PARENT_SCOPE)
set(MECAB_LIBRARIES ${MECAB_LIBRARIES} PARENT_SCOPE)

set(CABOCHA_FOUND ${CABOCHA_FOUND} PARENT_SCOPE)
set(CABOCHA_INCLUDE_DIRS ${CABOCHA_INCLUDE_DIRS} PARENT_SCOPE)
set(CABOCHA_LIBRARIES ${CABOCHA_LINK_LIBRARIES} PARENT_SCOPE)
set(CABOCHA_LIBRARY_DIRS ${CABOCHA_LIBRARY_DIRS} PARENT_SCOPE)

set(CRFPP_FOUND ${CRFPP_FOUND} PARENT_SCOPE)
set(CRFPP_INCLUDE_DIRS ${CRFPP_INCLUDE_DIRS} PARENT_SCOPE)
set(CRFPP_LIBRARIES ${CRFPP_LIBRARIES} PARENT_SCOPE)

if(MECAB_FOUND)
    add_library(mecab_system INTERFACE IMPORTED GLOBAL)

    if(MECAB_CONFIG)
        if(MECAB_INCLUDE_DIRS)
            target_include_directories(mecab_system INTERFACE ${MECAB_INCLUDE_DIRS})
        endif()
        if(MECAB_LIBRARY_DIRS)
            target_link_directories(mecab_system INTERFACE ${MECAB_LIBRARY_DIRS})
        endif()
        if(MECAB_LINK_LIBRARIES)
            target_link_libraries(mecab_system INTERFACE ${MECAB_LINK_LIBRARIES})
        endif()
    else()
        target_include_directories(mecab_system INTERFACE ${MECAB_INCLUDE_DIRS})
        target_link_libraries(mecab_system INTERFACE ${MECAB_LIBRARIES})
    endif()
    message(STATUS "mecab_systemターゲットを作成")
else()
    message(FATAL_ERROR "MeCabが見つかりません。MeCab開発ライブラリをインストールしてください。")
endif()

add_library(cabocha_system INTERFACE IMPORTED GLOBAL)

if(CABOCHA_INCLUDE_DIRS)
    target_include_directories(cabocha_system INTERFACE ${CABOCHA_INCLUDE_DIRS})
endif()
if(CABOCHA_LIBRARY_DIRS)
    target_link_directories(cabocha_system INTERFACE ${CABOCHA_LIBRARY_DIRS})
endif()
if(CABOCHA_LINK_LIBRARIES)
    target_link_libraries(cabocha_system INTERFACE ${CABOCHA_LINK_LIBRARIES})
elseif(CABOCHA_LIBRARIES)
    target_link_libraries(cabocha_system INTERFACE ${CABOCHA_LIBRARIES})
endif()

if(CRFPP_FOUND AND CRFPP_LIBRARIES)
    get_filename_component(CRFPP_LIB_DIR ${CRFPP_LIBRARIES} DIRECTORY)
    target_link_directories(cabocha_system INTERFACE ${CRFPP_LIB_DIR})
    target_link_libraries(cabocha_system INTERFACE ${CRFPP_LIBRARIES})
endif()

message(STATUS "cabocha_systemターゲットを作成")

message(STATUS "システムライブラリ検索が完了:")
message(STATUS "  MeCab: ${MECAB_FOUND}")
message(STATUS "  CaboCha: ${CABOCHA_FOUND}")
message(STATUS "  CRF++: ${CRFPP_FOUND}")
