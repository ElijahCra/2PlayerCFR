add_library(HandAbstraction STATIC hand_index.c deck.c)
target_link_libraries(HandAbstraction PRIVATE m)
target_include_directories(HandAbstraction PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})