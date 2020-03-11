# CMakeList.txt : CMake project for EGL2, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)


project ("EGL2")


# Add source to this project's executable.
aux_source_directory(memfs FILE_SOURCES)
message(STATUS ${FILE_SOURCES})
add_executable (EGL2 ${FILE_SOURCES})#"passthrough.c")

set_property(TARGET EGL2 PROPERTY CXX_STANDARD 20)

message(STATUS $ENV{ProgramFiles\(x86\)})
target_include_directories(EGL2 PRIVATE "$ENV{ProgramFiles\(x86\)}\\WinFsp\\inc")
target_link_libraries(EGL2 "$ENV{ProgramFiles\(x86\)}\\WinFsp\\lib\\winfsp-x64.lib" Shlwapi)
configure_file(winfsp-x64.dll winfsp-x64.dll COPYONLY)

# TODO: Add tests and install targets if needed.
