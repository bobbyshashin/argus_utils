cmake_minimum_required(VERSION 2.8.3)
project(broadcast)

set(CMAKE_CXX_FLAGS "-g -Wall -std=c++0x")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/")

set(CMAKE_BUILD_TYPE Release)

find_package(catkin REQUIRED 
        COMPONENTS      message_generation
                        std_msgs
                        roscpp
                        rospy
                        argus_utils
                        lookup
)

# Use Boost for most utilities, threading
find_package(Boost REQUIRED)

add_message_files( FILES FloatVectorStamped.msg )

add_service_files( FILES QueryFeatures.srv )

catkin_python_setup()

generate_messages( DEPENDENCIES
                  std_msgs )

catkin_package( INCLUDE_DIRS     include

                LIBRARIES        broadcast

                CATKIN_DEPENDS   message_runtime
                                 std_msgs
                                 roscpp 
                                 rospy
                                 argus_utils
                                 lookup

                DEPENDS          Boost 
                                 system_lib
)

include_directories( include
                    ${Boost_INCLUDE_DIRS}
                    ${catkin_INCLUDE_DIRS}
                    )

add_library( broadcast
            src/BroadcastCommon.cpp
            src/BroadcastInfoManager.cpp
            src/BroadcastMultiReceiver.cpp
            src/BroadcastTransmitter.cpp
            src/BroadcastReceiver.cpp
            )
target_link_libraries( broadcast
                      ${Boost_LIBRARIES}
                      ${catkin_LIBRARIES}
                      )
add_dependencies( broadcast covreg_gencfg )

add_executable( broadcast_listener nodes/broadcast_listener.cpp )
target_link_libraries( broadcast_listener ${catkin_LIBRARIES} broadcast )


## Mark executables and/or libraries for installation
install(TARGETS broadcast broadcast_listener
        ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
        LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
        RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
        )

install(DIRECTORY include/${PROJECT_NAME}/
        DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
        FILES_MATCHING PATTERN "*.h"
        )
