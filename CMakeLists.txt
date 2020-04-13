cmake_minimum_required (VERSION 3.9.2)

project (EGL2) 

set(WITH_GUI ON CACHE BOOL "Compile with a GUI" FORCE)
set(WX_DIR "J:\\Code\\wxWidgets" CACHE PATH "wxWidgets directory" FORCE)

aux_source_directory(. FILE_SOURCES)
aux_source_directory(filesystem FILESYSTEM_FILE_SOURCES)
aux_source_directory(storage STORAGE_FILE_SOURCES)
aux_source_directory(web WEB_FILE_SOURCES)
aux_source_directory(checks CHECKS_FILE_SOURCES)

if (WITH_GUI)
  message("Building with GUI")

  aux_source_directory(gui INTERFACE_FILE_SOURCES)
  add_executable(EGL2 WIN32 ${INTERFACE_FILE_SOURCES} "gui/resources.rc" ${FILESYSTEM_FILE_SOURCES} ${STORAGE_FILE_SOURCES} ${WEB_FILE_SOURCES} ${CHECKS_FILE_SOURCES} ${FILE_SOURCES})

  set(wxWidgets_ROOT_DIR "${WX_DIR}")
  set(wxWidgets_LIB_DIR "${WX_DIR}/lib/vc_x64_lib")
  set(wxWidgets_EXCLUDE_COMMON_LIBRARIES TRUE)

  if (CMAKE_BUILD_TYPE EQUAL "DEBUG")
    set(wxWidgets_USE_DEBUG ON)
  else()
    set(wxWidgets_USE_DEBUG OFF)
  endif()

  set(wxWidgets_USE_STATIC ON)
  set(wxWidgets_USE_UNICODE ON)

  find_package(wxWidgets REQUIRED COMPONENTS core base png zlib)
  include(${wxWidgets_USE_FILE})
  target_link_libraries(EGL2 PUBLIC ${wxWidgets_LIBRARIES})
else()
  message("Building without GUI")

  aux_source_directory(cmd INTERFACE_FILE_SOURCES)
  add_executable(EGL2 ${INTERFACE_FILE_SOURCES} ${FILESYSTEM_FILE_SOURCES} ${STORAGE_FILE_SOURCES} ${WEB_FILE_SOURCES} ${CHECKS_FILE_SOURCES} ${FILE_SOURCES})
endif()

set_property(TARGET EGL2 PROPERTY CXX_STANDARD 20)

find_package(OpenSSL REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(lz4 REQUIRED)

set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        )
foreach(CompilerFlag ${CompilerFlags})
  string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
endforeach()

target_link_options(EGL2 PRIVATE "/DELAYLOAD:winfsp-x64.dll")
target_include_directories(EGL2 PRIVATE "$ENV{ProgramFiles\(x86\)}\\WinFsp\\inc" ${RAPIDJSON_INCLUDE_DIRS} "libdeflate")
target_link_libraries(EGL2 PRIVATE "$ENV{ProgramFiles\(x86\)}\\WinFsp\\lib\\winfsp-x64.lib" OpenSSL::SSL OpenSSL::Crypto Crypt32 ZLIB::ZLIB lz4::lz4 delayimp "${CMAKE_CURRENT_SOURCE_DIR}\\libdeflate\\libdeflatestatic.lib")