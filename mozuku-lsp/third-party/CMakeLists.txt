cmake_minimum_required(VERSION 3.16)

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(TREESITTER REQUIRED tree-sitter)

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
    message(STATUS "mecab-configが見つかりません - 標準システムパスを試行中")
    find_path(MECAB_INCLUDE_DIRS mecab.h
        PATHS
            /opt/homebrew/include
            /opt/homebrew/Cellar/mecab/*/include
            /usr/local/include
            /usr/include
            "C:/Program Files/MeCab/sdk"
            "C:/mecab/sdk"
        NO_DEFAULT_PATH)
    find_library(MECAB_LIBRARIES mecab
        NAMES mecab libmecab
        PATHS
            /opt/homebrew/lib
            /opt/homebrew/Cellar/mecab/*/lib
            /usr/local/lib
            /usr/lib
            "C:/Program Files/MeCab/sdk"
            "C:/mecab/sdk"
        NO_DEFAULT_PATH)
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
        COMMAND ${CABOCHA_CONFIG} --libs
        OUTPUT_VARIABLE CABOCHA_LIBRARIES
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${CABOCHA_CONFIG} --cflags
        OUTPUT_VARIABLE CABOCHA_CFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    string(REGEX MATCHALL "-I[^ ]*" CABOCHA_INCLUDE_FLAGS "${CABOCHA_CFLAGS}")
    set(CABOCHA_INCLUDE_DIRS "")
    foreach(flag ${CABOCHA_INCLUDE_FLAGS})
        string(SUBSTRING ${flag} 2 -1 include_dir)
        list(APPEND CABOCHA_INCLUDE_DIRS ${include_dir})
    endforeach()

    string(REGEX MATCHALL "-l[^ ]*" CABOCHA_LIB_FLAGS "${CABOCHA_LIBRARIES}")
    string(REGEX MATCHALL "-L[^ ]*" CABOCHA_LIB_DIRS "${CABOCHA_LIBRARIES}")

    set(CABOCHA_CLEANED_LIBS "")
    foreach(lib ${CABOCHA_LIB_FLAGS})
        string(SUBSTRING ${lib} 2 -1 clean_lib)
        list(APPEND CABOCHA_CLEANED_LIBS ${clean_lib})
    endforeach()

    set(CABOCHA_CLEANED_DIRS "")
    foreach(dir ${CABOCHA_LIB_DIRS})
        string(SUBSTRING ${dir} 2 -1 clean_dir)
        list(APPEND CABOCHA_CLEANED_DIRS ${clean_dir})
    endforeach()

    set(CABOCHA_LIBRARY_DIRS ${CABOCHA_CLEANED_DIRS})
    set(CABOCHA_LINK_LIBRARIES ${CABOCHA_CLEANED_LIBS})

    set(CABOCHA_FOUND TRUE)
    message(STATUS "cabocha-configによりCaboChaを発見:")
    message(STATUS "  インクルードディレクトリ: ${CABOCHA_INCLUDE_DIRS}")
    message(STATUS "  ライブラリ: ${CABOCHA_LIBRARIES}")
else()
    message(STATUS "cabocha-configが見つかりません - 標準システムパスを試行中")
    find_path(CABOCHA_INCLUDE_DIRS cabocha.h
        PATHS
            /opt/homebrew/include
            /opt/homebrew/Cellar/cabocha/*/include
            /usr/local/include
            /usr/include
            "C:/Program Files/CaboCha/sdk"
            "C:/cabocha/sdk"
        NO_DEFAULT_PATH)
    find_library(CABOCHA_LIBRARIES cabocha
        NAMES cabocha libcabocha
        PATHS
            /opt/homebrew/lib
            /opt/homebrew/Cellar/cabocha/*/lib
            /usr/local/lib
            /usr/lib
            "C:/Program Files/CaboCha/sdk"
            "C:/cabocha/sdk"
        NO_DEFAULT_PATH)
    if(CABOCHA_INCLUDE_DIRS AND CABOCHA_LIBRARIES)
        set(CABOCHA_FOUND TRUE)
        message(STATUS "システムパスでCaboChaを発見:")
        message(STATUS "  ヘッダー: ${CABOCHA_INCLUDE_DIRS}")
        message(STATUS "  ライブラリ: ${CABOCHA_LIBRARIES}")
    endif()
endif()

find_path(CRFPP_INCLUDE_DIRS crfpp.h
    PATHS
        /opt/homebrew/include
        /opt/homebrew/Cellar/crf++/*/include
        /usr/local/include
        /usr/include
        "C:/Program Files/CRF++/sdk"
        "C:/crfpp/sdk"
    PATH_SUFFIXES crf++
    NO_DEFAULT_PATH)

find_library(CRFPP_LIBRARIES crfpp
    NAMES crfpp libcrfpp
    PATHS
        /opt/homebrew/lib
        /opt/homebrew/Cellar/crf++/*/lib
        /usr/local/lib
        /usr/lib
        "C:/Program Files/CRF++/sdk"
        "C:/crfpp/sdk"
    NO_DEFAULT_PATH)

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
set(CABOCHA_LIBRARIES ${CABOCHA_LIBRARIES} PARENT_SCOPE)

set(CRFPP_FOUND ${CRFPP_FOUND} PARENT_SCOPE)
set(CRFPP_INCLUDE_DIRS ${CRFPP_INCLUDE_DIRS} PARENT_SCOPE)
set(CRFPP_LIBRARIES ${CRFPP_LIBRARIES} PARENT_SCOPE)

set(TREESITTER_FOUND ${TREESITTER_FOUND} PARENT_SCOPE)
set(TREESITTER_INCLUDE_DIRS ${TREESITTER_INCLUDE_DIRS} PARENT_SCOPE)
set(TREESITTER_LIBRARIES ${TREESITTER_LIBRARIES} PARENT_SCOPE)
set(TREESITTER_CFLAGS_OTHER ${TREESITTER_CFLAGS_OTHER} PARENT_SCOPE)

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
    message(FATAL_ERROR "MeCabが見つかりません - MeCab開発ライブラリをインストールしてください")
endif()

if(CABOCHA_FOUND)
    add_library(cabocha_system INTERFACE IMPORTED GLOBAL)

    if(CABOCHA_CONFIG)
        if(CABOCHA_INCLUDE_DIRS)
            target_include_directories(cabocha_system INTERFACE ${CABOCHA_INCLUDE_DIRS})
        endif()
        if(CABOCHA_LIBRARY_DIRS)
            target_link_directories(cabocha_system INTERFACE ${CABOCHA_LIBRARY_DIRS})
        endif()
        if(CABOCHA_LINK_LIBRARIES)
            target_link_libraries(cabocha_system INTERFACE ${CABOCHA_LINK_LIBRARIES})
        endif()
    else()
        target_include_directories(cabocha_system INTERFACE ${CABOCHA_INCLUDE_DIRS})
        target_link_libraries(cabocha_system INTERFACE ${CABOCHA_LIBRARIES})
    endif()

    if(CRFPP_FOUND AND CRFPP_LIBRARIES)
        get_filename_component(CRFPP_LIB_DIR ${CRFPP_LIBRARIES} DIRECTORY)
        target_link_directories(cabocha_system INTERFACE ${CRFPP_LIB_DIR})
    endif()

    message(STATUS "cabocha_systemターゲットを作成")
else()
    add_library(cabocha_system INTERFACE IMPORTED GLOBAL)
    target_link_libraries(cabocha_system INTERFACE mecab_system)
    message(STATUS "CaboChaが見つかりません - MeCabのみモードを使用")
endif()

set(LATEX_PARSER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-latex")
set(LATEX_PARSER_SRC "${LATEX_PARSER_DIR}/src/parser.c")

if(NOT EXISTS "${LATEX_PARSER_SRC}")
    find_program(TREE_SITTER_CLI tree-sitter REQUIRED)
    message(STATUS "tree-sitter-latex パーサーを生成中...")
    execute_process(
        COMMAND ${TREE_SITTER_CLI} generate
        WORKING_DIRECTORY ${LATEX_PARSER_DIR}
        RESULT_VARIABLE GENERATE_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(NOT GENERATE_RESULT EQUAL 0)
        message(FATAL_ERROR "tree-sitter-latex パーサーの生成に失敗しました")
    endif()
    message(STATUS "tree-sitter-latex パーサーの生成に成功")
else()
    message(STATUS "tree-sitter-latex パーサーは既に存在します")
endif()

message(STATUS "システムライブラリ検索が完了:")
message(STATUS "  MeCab: ${MECAB_FOUND}")
message(STATUS "  CaboCha: ${CABOCHA_FOUND}")
message(STATUS "  CRF++: ${CRFPP_FOUND}")
