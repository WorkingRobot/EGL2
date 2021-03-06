cmake_minimum_required (VERSION 3.9.2)

project (EGL2) 

set(WX_DIR "J:\\Code\\wxWidgets" CACHE PATH "wxWidgets directory" FORCE)
option(ENABLE_CONSOLE "Use console subsystem" OFF)
option(ENABLE_SIGNING "Sign the executable" OFF)

aux_source_directory(. FILE_SOURCES)
aux_source_directory(filesystem FILESYSTEM_FILE_SOURCES)
aux_source_directory(storage STORAGE_FILE_SOURCES)
aux_source_directory(web WEB_FILE_SOURCES)
aux_source_directory(web/personal PERSONAL_FILE_SOURCES)
aux_source_directory(web/manifest MANIFEST_FILE_SOURCES)
aux_source_directory(web/http HTTP_FILE_SOURCES)
aux_source_directory(checks CHECKS_FILE_SOURCES)
aux_source_directory(gui INTERFACE_FILE_SOURCES)
aux_source_directory(libraries/curlion CURLION_FILE_SOURCES)

aux_source_directory(localetool LOCALETOOL_FILE_SOURCES)

if (ENABLE_CONSOLE)
set(SUBSYSTEM "")
add_compile_definitions(USE_CONSOLE)
message(STATUS "Using console subsystem")
else()
set (SUBSYSTEM "WIN32")
message(STATUS "Using GUI subsystem")
endif()

add_executable(EGL2 ${SUBSYSTEM}
        ${INTERFACE_FILE_SOURCES}
        "resources.rc"
        ${CURLION_FILE_SOURCES}
        ${FILESYSTEM_FILE_SOURCES}
        ${STORAGE_FILE_SOURCES}
        ${WEB_FILE_SOURCES}
        ${MANIFEST_FILE_SOURCES}
        ${PERSONAL_FILE_SOURCES}
        ${HTTP_FILE_SOURCES}
        ${CHECKS_FILE_SOURCES}
        ${FILE_SOURCES})

add_executable(LocaleTool
        ${LOCALETOOL_FILE_SOURCES})

set(wxWidgets_ROOT_DIR "${WX_DIR}")
set(wxWidgets_LIB_DIR "${WX_DIR}/lib/vc_x64_lib")
set(wxWidgets_EXCLUDE_COMMON_LIBRARIES TRUE)
set(wxWidgets_USE_STATIC ON)
set(wxWidgets_USE_UNICODE ON)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
set(wxWidgets_USE_DEBUG ON)
else()
set(wxWidgets_USE_DEBUG OFF)
endif()

set(Boost_USE_STATIC_LIBS ON)
add_definitions(-DBOOST_ASIO_DISABLE_CONCEPTS)

set_property(TARGET EGL2 PROPERTY CXX_STANDARD 20)
set_property(TARGET LocaleTool PROPERTY CXX_STANDARD 20)

find_package(OpenSSL REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)
find_package(lz4 REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(LibLZMA CONFIG REQUIRED)
find_package(BOOST REQUIRED COMPONENTS asio)
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

if (ENABLE_SIGNING)
message(STATUS "Signing files")
file(READ "cert.pwd" SIGN_PASS)
add_custom_command(TARGET EGL2 POST_BUILD
    COMMAND
        signtool sign /f "${CMAKE_SOURCE_DIR}/cert.pfx" /p "${SIGN_PASS}" /fd sha1 /t http://timestamp.digicert.com /v $<TARGET_FILE:EGL2> &&
        signtool sign /f "${CMAKE_SOURCE_DIR}/cert.pfx" /p "${SIGN_PASS}" /fd sha256 /tr http://timestamp.digicert.com?td=sha256 /td sha256 /as /v $<TARGET_FILE:EGL2>
)
endif()

set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS_MINSIZEREL
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS_MINSIZEREL
        CMAKE_C_FLAGS_RELEASE
        )
foreach(CompilerFlag ${CompilerFlags})
  string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
endforeach()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC" AND CMAKE_BUILD_TYPE MATCHES "Release")
   target_compile_options(EGL2 PRIVATE /Zi)
    set_target_properties(EGL2 PROPERTIES
        LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF"
        COMPILE_PDB_NAME EGL2 
        COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
    )
endif()

target_link_options(EGL2 PRIVATE "/DELAYLOAD:winfsp-x64.dll" "/DELAYLOAD:oo2core_8_win64.dll")
target_include_directories(EGL2 PRIVATE
    "$ENV{ProgramFiles\(x86\)}\\WinFsp\\inc"
    ${Boost_LIBRARIES}
    ${RAPIDJSON_INCLUDE_DIRS}
    "libraries\\libdeflate"
    "libraries\\curlion"
    "libraries\\oodle")
target_link_libraries(EGL2 PRIVATE
    "$ENV{ProgramFiles\(x86\)}\\WinFsp\\lib\\winfsp-x64.lib"
    ${wxWidgets_LIBRARIES}
    CURL::libcurl
    OpenSSL::Crypto
    Crypt32
    libzstd
    lz4::lz4
    LibLZMA::LibLZMA
    Htmlhelp
    delayimp
    "${CMAKE_CURRENT_SOURCE_DIR}\\libraries\\libdeflate\\libdeflatestatic.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}\\libraries\\oodle\\oo2core_8_win64.lib")

target_link_libraries(LocaleTool PRIVATE libzstd)