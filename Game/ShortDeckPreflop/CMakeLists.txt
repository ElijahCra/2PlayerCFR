add_library(ShortDeckPreflop STATIC Game.cpp ConcreteGameStates.cpp)

target_include_directories(ShortDeckPreflop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})