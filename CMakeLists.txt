cmake_minimum_required(VERSION 3.6)
project(mpmc_example)

set(CMAKE_CXX_STANDARD 14)

set(boostinclude /usr/local/Cellar/boost160/1.60.0/include)
set(boostlib /usr/local/Cellar/boost160/1.60.0/lib)
set(Boost_LIBRARIES -L${boostlib} -lboost_thread-mt -lboost_filesystem-mt -lboost_system-mt -lboost_chrono-mt -lboost_atomic-mt -lboost_program_options)
set(junctionlib /Users/dantrim/workarea/mpmc_example/preshing/junction/build)
set(turflib /Users/dantrim/workarea/mpmc_example/preshing/junction/build/turf)
set(Junction_LIBRARIES -L${junctionlib} -ljunction)
set(Turf_LIBRARIES -L${turflib} -lturf)

include_directories(include/)
include_directories(./spdlog/include/)
include_directories(./preshing/junction/)
include_directories(./preshing/turf/)
include_directories(/Users/dantrim/workarea/mpmc_example/libcuckoo)

set(SOURCE_FILES
    # main
    src/main.cpp
    # stuff
    src/data_listener.cpp
    src/data_builder.cpp
    src/event_builder.cpp
)

set(HEADER_FILES
    include/data_fragment.h
    include/data_listener.h
    include/data_builder.h
    include/event_builder.h
    include/map_defs.h
)

set(SOURCE_FILES_EX
    src/main_sender.cpp
)



add_executable(mpmc_test ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(mpmc_test ${Boost_LIBRARIES} ${Junction_LIBRARIES} ${Turf_LIBRARIES})
#target_link_libraries(mpmc_test ${BOOST_LIBRARIES})

add_executable(sender ${SOURCE_FILES_EX})
target_link_libraries(sender ${Boost_LIBRARIES})
