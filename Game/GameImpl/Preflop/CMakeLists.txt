add_subdirectory(PreCards)

add_library(Preflop STATIC Game.cpp ConcreteGameStates.cpp)

target_include_directories(Preflop PUBLIC .)

target_link_libraries(Preflop PUBLIC PreCards)