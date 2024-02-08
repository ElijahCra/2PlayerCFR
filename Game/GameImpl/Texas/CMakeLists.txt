add_library(Texas STATIC Game.cpp ConcreteGameStates.cpp)

target_include_directories(Texas PUBLIC .)

target_link_libraries(Texas PUBLIC Cards)