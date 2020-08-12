cmake_minimum_required (VERSION 3.0)

project (Robot-Web-Server)

add_compile_options(-std=c++17 -Wall -Wextra -Wsign-conversion)

# Robot web server
add_executable ( web_server ./src/server.cpp )

set ( CEREAL_INCLUDE_DIR "../cereal/include" CACHE FILEPATH "Cereal include folder" )
target_include_directories ( web_server PUBLIC ../robot-comm/core
                                        PUBLIC ../robot-comm/msgs
                                        PUBLIC ./include
                                        ${CEREAL_INCLUDE_DIR} )
target_link_libraries ( web_server robot-comm )

find_package ( Boost 1.53.0 COMPONENTS system thread filesystem REQUIRED )
target_include_directories ( web_server PRIVATE ${Boost_INCLUDE_DIR} )
target_link_libraries ( web_server ${Boost_LIBRARIES} )

install ( TARGETS web_server RUNTIME DESTINATION /usr/bin )
