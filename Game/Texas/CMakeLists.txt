add_library(Texas STATIC Game.cpp ConcreteGameStates.cpp)

target_include_directories(Texas PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})