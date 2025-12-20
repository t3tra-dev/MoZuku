cmake_minimum_required(VERSION 3.16)
project(mozuku-lsp C CXX)

if(POLICY CMP0177)
  cmake_policy(SET CMP0177 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third-party/json")
    message(STATUS "システムのnlohmann_jsonが見つかりません。third-partyに存在するnlohmann_jsonを使用します。")
    add_subdirectory(third-party/json EXCLUDE_FROM_ALL)
  else()
    message(FATAL_ERROR "nlohmann_json が見つかりません。システムにインストールするか、third-party/json submodulesを初期化してください。")
  endif()
else()
  message(STATUS "システムのnlohmann_jsonを使用します")
endif()

add_subdirectory(third-party)

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(TREESITTER tree-sitter)
endif()

if(NOT TREESITTER_FOUND)
  find_library(TREESITTER_LIBRARIES
      NAMES tree-sitter
  )
  find_path(TREESITTER_INCLUDE_DIRS NAMES tree_sitter/api.h)

  if(TREESITTER_LIBRARIES AND TREESITTER_INCLUDE_DIRS)
    set(TREESITTER_FOUND TRUE)
  endif()
endif()

if(TREESITTER_FOUND)
  message(STATUS "tree-sitter found:")
  message(STATUS "  include: ${TREESITTER_INCLUDE_DIRS}")
  message(STATUS "  library: ${TREESITTER_LIBRARIES}")
else()
  message(FATAL_ERROR "tree-sitterが見つかりません。tree-sitterがインストールされているか確認してください。")
endif()

find_package(CURL REQUIRED)

function(add_tree_sitter_language target_name source_root)
  set(src_dir "${source_root}/src")
  set(sources "${src_dir}/parser.c")
  if(EXISTS "${src_dir}/scanner.c")
    list(APPEND sources "${src_dir}/scanner.c")
  endif()

  add_library(${target_name} STATIC ${sources})
  target_include_directories(${target_name} PUBLIC
    "${src_dir}"
    "${src_dir}/tree_sitter")
  set_target_properties(${target_name} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    LINKER_LANGUAGE C)
endfunction()

function(add_tree_sitter_language_with_fallback target_name package_name source_root)
  file(GLOB NIX_PARSER_PATHS "/nix/store/*-${package_name}-grammar-*/parser")
  if(NIX_PARSER_PATHS)
    list(GET NIX_PARSER_PATHS 0 NIX_PARSER_PATH)
    message(STATUS "Nixの${package_name}を使用します: ${NIX_PARSER_PATH}")
    add_library(${target_name} UNKNOWN IMPORTED GLOBAL)
    set_target_properties(${target_name} PROPERTIES
      IMPORTED_LOCATION ${NIX_PARSER_PATH}
    )
    return()
  endif()

  find_library(${target_name}_SYSTEM_LIB
    NAMES ${package_name}
    PATH_SUFFIXES lib
  )

  if(${target_name}_SYSTEM_LIB)
    message(STATUS "システムの${package_name}を使用します: ${${target_name}_SYSTEM_LIB}")
    add_library(${target_name} INTERFACE IMPORTED GLOBAL)
    target_link_libraries(${target_name} INTERFACE ${${target_name}_SYSTEM_LIB})
  else()
    if(EXISTS "${source_root}/src/parser.c")
      message(STATUS "third-partyから${package_name}をビルドします")
      add_tree_sitter_language(${target_name} "${source_root}")
    else()
      message(WARNING "${package_name}が見つかりません。システムにもthird-partyにもライブラリが存在しません。")
      add_library(${target_name} INTERFACE IMPORTED GLOBAL)
    endif()
  endif()
endfunction()

add_tree_sitter_language_with_fallback(tree_sitter_lang_c "tree-sitter-c" "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-c")
add_tree_sitter_language_with_fallback(tree_sitter_lang_cpp "tree-sitter-cpp" "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-cpp")
add_tree_sitter_language_with_fallback(tree_sitter_lang_html "tree-sitter-html" "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-html")
add_tree_sitter_language_with_fallback(tree_sitter_lang_javascript "tree-sitter-javascript" "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-javascript")
add_tree_sitter_language_with_fallback(tree_sitter_lang_python "tree-sitter-python" "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-python")
add_tree_sitter_language_with_fallback(tree_sitter_lang_rust "tree-sitter-rust" "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-rust")
add_tree_sitter_language_with_fallback(tree_sitter_lang_typescript "tree-sitter-typescript" "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-typescript/typescript")
add_tree_sitter_language_with_fallback(tree_sitter_lang_tsx "tree-sitter-tsx" "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-typescript/tsx")
add_tree_sitter_language_with_fallback(tree_sitter_lang_latex "tree-sitter-latex" "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-latex")

set(MOZUKU_SOURCES
  src/main.cpp
  src/lsp.cpp
  src/utf16.cpp
  src/analyzer.cpp
  src/encoding_utils.cpp
  src/text_processor.cpp
  src/pos_analyzer.cpp
  src/mecab_manager.cpp
  src/grammar_checker.cpp
  src/wikipedia.cpp
  src/comment_extractor.cpp
)

add_executable(mozuku-lsp ${MOZUKU_SOURCES})

target_include_directories(mozuku-lsp PRIVATE include)

if(WIN32)
  target_compile_definitions(mozuku-lsp PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

target_link_libraries(mozuku-lsp PRIVATE
  nlohmann_json::nlohmann_json
  tree_sitter_lang_c
  tree_sitter_lang_cpp
  tree_sitter_lang_html
  tree_sitter_lang_javascript
  tree_sitter_lang_python
  tree_sitter_lang_rust
  tree_sitter_lang_typescript
  tree_sitter_lang_tsx
  tree_sitter_lang_latex
  mecab_system
  CURL::libcurl
)

target_link_libraries(mozuku-lsp PRIVATE cabocha_system ${CABOCHA_LIBRARIES})
target_include_directories(mozuku-lsp PRIVATE ${TREESITTER_INCLUDE_DIRS})
target_compile_options(mozuku-lsp PRIVATE ${TREESITTER_CFLAGS_OTHER})
target_link_libraries(mozuku-lsp PRIVATE ${TREESITTER_LIBRARIES})
if(TREESITTER_LIBRARY_DIRS)
  target_link_directories(mozuku-lsp PRIVATE ${TREESITTER_LIBRARY_DIRS})
endif()

find_package(Threads REQUIRED)
target_link_libraries(mozuku-lsp PRIVATE Threads::Threads)

if(APPLE)
    target_link_libraries(mozuku-lsp PRIVATE "-liconv")
elseif(WIN32)
    find_package(Iconv REQUIRED)
    target_link_libraries(mozuku-lsp PRIVATE Iconv::Iconv)
elseif(UNIX)
    find_library(RT_LIBRARY rt)
    if(RT_LIBRARY)
        target_link_libraries(mozuku-lsp PRIVATE ${RT_LIBRARY})
    endif()
endif()

message(STATUS "システムライブラリ統合:")
message(STATUS "  MeCab: ${MECAB_FOUND}")
message(STATUS "  CaboCha: ${CABOCHA_FOUND}")
message(STATUS "  CURL: ${CURL_FOUND}")

set(VSCODE_EXTENSION_DIR "${CMAKE_SOURCE_DIR}/../vscode-mozuku")

install(TARGETS mozuku-lsp
    RUNTIME DESTINATION "${VSCODE_EXTENSION_DIR}/bin"
)

install(TARGETS mozuku-lsp
        RUNTIME DESTINATION bin)


add_custom_target(package-extension
    COMMAND ${CMAKE_COMMAND} --build . --target install
    COMMENT "VS Code拡張をLSPサーバーと共にパッケージ化中 (システムライブラリが必要) "
    DEPENDS mozuku-lsp
)

message(STATUS "VS Code拡張のパッケージ先: ${VSCODE_EXTENSION_DIR}")
message(STATUS "注意: ターゲットシステムにMeCab/CaboCha/CURLライブラリのインストールが必要です")
