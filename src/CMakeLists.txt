file(GLOB examples ${CMAKE_CURRENT_LIST_DIR}/*)
list(REMOVE_ITEM examples ${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt)

foreach(example ${examples})
    get_filename_component(exe ${example} NAME_WE)
    add_executable(${exe} ${exe}.cc)
    target_link_libraries(${exe} GLESv2 EGL ${CONAN_LIBS})
endforeach()
