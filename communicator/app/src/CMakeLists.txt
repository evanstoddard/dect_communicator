target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(ble)
add_subdirectory(dect)
add_subdirectory(endpoints)
add_subdirectory(utils)
