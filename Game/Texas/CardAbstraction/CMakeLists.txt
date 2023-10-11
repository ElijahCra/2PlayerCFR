add_library(hand STATIC deck.c hand_index.c)
target_include_directories(hand PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(check-main check-main.c)
target_link_libraries(check-main hand)