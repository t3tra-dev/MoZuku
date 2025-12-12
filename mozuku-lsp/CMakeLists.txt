cmake_minimum_required(VERSION 3.16)
project(mozuku-lsp C CXX)

if(POLICY CMP0177)
  cmake_policy(SET CMP0177 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)

option(BUILD_WITH_CABOCHA "Build with CaboCha dependency parsing" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(third-party/json EXCLUDE_FROM_ALL)

# vcpkg の unofficial-tree-sitter を先に試す（IMPORTED ターゲットを親スコープで利用可能にする）
find_package(unofficial-tree-sitter CONFIG QUIET)

add_subdirectory(third-party)

if(NOT TREESITTER_FOUND)
  message(FATAL_ERROR "tree-sitter が検出できませんでした。pkg-config に tree-sitter パッケージが登録されているか確認してください。")
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

add_tree_sitter_language(tree_sitter_lang_c "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-c")
add_tree_sitter_language(tree_sitter_lang_cpp "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-cpp")
add_tree_sitter_language(tree_sitter_lang_html "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-html")
add_tree_sitter_language(tree_sitter_lang_javascript "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-javascript")
add_tree_sitter_language(tree_sitter_lang_python "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-python")
add_tree_sitter_language(tree_sitter_lang_rust "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-rust")
add_tree_sitter_language(tree_sitter_lang_typescript "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-typescript/typescript")
add_tree_sitter_language(tree_sitter_lang_tsx "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-typescript/tsx")
add_tree_sitter_language(tree_sitter_lang_latex "${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-latex")

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

if(CABOCHA_FOUND)
  target_link_libraries(mozuku-lsp PRIVATE cabocha_system ${CABOCHA_LIBRARIES})
endif()
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

add_custom_target(package-extension
    COMMAND ${CMAKE_COMMAND} --build . --target install
    COMMENT "VS Code拡張をLSPサーバーと共にパッケージ化中 (システムライブラリが必要) "
    DEPENDS mozuku-lsp
)

message(STATUS "VS Code拡張のパッケージ先: ${VSCODE_EXTENSION_DIR}")
message(STATUS "注意: ターゲットシステムにMeCab/CaboCha/CURLライブラリのインストールが必要です")
