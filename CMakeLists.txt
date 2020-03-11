cmake_minimum_required (VERSION 3.8)

project ("EGL2")

aux_source_directory(. FILE_SOURCES)
aux_source_directory(memfs MEMFS_FILE_SOURCES)

add_executable (EGL2 ${MEMFS_FILE_SOURCES} ${FILE_SOURCES})

set_property(TARGET EGL2 PROPERTY CXX_STANDARD 20)

target_include_directories(EGL2 PRIVATE "$ENV{ProgramFiles\(x86\)}\\WinFsp\\inc")
target_link_libraries(EGL2 "$ENV{ProgramFiles\(x86\)}\\WinFsp\\lib\\winfsp-x64.lib")
configure_file(winfsp-x64.dll winfsp-x64.dll COPYONLY)