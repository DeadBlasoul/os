file(GLOB_RECURSE App_SRC
    NAMES "*.c" "*.h" "*.cpp" "*.hpp")

find_package(TBB CONFIG REQUIRED)
if(NOT TBB_FOUND)
    message(FATAL_ERROR "Threding Building Blocks not found")
endif()

add_executable(App ${App_SRC})
target_include_directories(App PRIVATE ${TBB_IMPORTED_TARGETS})
target_link_libraries(App PRIVATE ${TBB_IMPORTED_TARGETS})
