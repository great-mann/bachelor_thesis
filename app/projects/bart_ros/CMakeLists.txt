cmake_minimum_required(VERSION 2.8.3)
project(bart_ros)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS
  roscpp
  std_msgs
  visualization_msgs
  cv_bridge
  dynamic_reconfigure
)

## System dependencies are found with CMake's conventions
# find_package(Boost REQUIRED COMPONENTS system)


## Uncomment this if the package has a setup.py. This macro ensures
## modules and global scripts declared therein get installed
## See http://ros.org/doc/api/catkin/html/user_guide/setup_dot_py.html
# catkin_python_setup()

################################################
## Declare ROS messages, services and actions ##
################################################

## To declare and build messages, services or actions from within this
## package, follow these steps:
## * Let MSG_DEP_SET be the set of packages whose message types you use in
##   your messages/services/actions (e.g. std_msgs, actionlib_msgs, ...).
## * In the file package.xml:
##   * add a build_depend tag for "message_generation"
##   * add a build_depend and a run_depend tag for each package in MSG_DEP_SET
##   * If MSG_DEP_SET isn't empty the following dependency has been pulled in
##     but can be declared for certainty nonetheless:
##     * add a run_depend tag for "message_runtime"
## * In this file (CMakeLists.txt):
##   * add "message_generation" and every package in MSG_DEP_SET to
##     find_package(catkin REQUIRED COMPONENTS ...)
##   * add "message_runtime" and every package in MSG_DEP_SET to
##     catkin_package(CATKIN_DEPENDS ...)
##   * uncomment the add_*_files sections below as needed
##     and list every .msg/.srv/.action file to be processed
##   * uncomment the generate_messages entry below
##   * add every package in MSG_DEP_SET to generate_messages(DEPENDENCIES ...)

## Generate messages in the 'msg' folder
# add_message_files(
#   FILES
#   Message1.msg
#   Message2.msg
# )

## Generate services in the 'srv' folder
# add_service_files(
#   FILES
#   Service1.srv
#   Service2.srv
# )

## Generate actions in the 'action' folder
# add_action_files(
#   FILES
#   Action1.action
#   Action2.action
# )

## Generate added messages and services with any dependencies listed here
# generate_messages(
#   DEPENDENCIES
#   std_msgs#   visualization_msgs
# )

################################################
## Declare ROS dynamic reconfigure parameters ##
################################################

## To declare and build dynamic reconfigure parameters within this
## package, follow these steps:
## * In the file package.xml:
##   * add a build_depend and a run_depend tag for "dynamic_reconfigure"
## * In this file (CMakeLists.txt):
##   * add "dynamic_reconfigure" to
##     find_package(catkin REQUIRED COMPONENTS ...)
##   * uncomment the "generate_dynamic_reconfigure_options" section below
##     and list every .cfg file to be processed

## Generate dynamic reconfigure parameters in the 'cfg' folder
# generate_dynamic_reconfigure_options(
#   cfg/DynReconf1.cfg
#   cfg/DynReconf2.cfg
# )

generate_dynamic_reconfigure_options(
  cfg/bart_ros.cfg
)

###################################
## catkin specific configuration ##
###################################
## The catkin_package macro generates cmake config files for your package
## Declare things to be passed to dependent projects
## INCLUDE_DIRS: uncomment this if you package contains header files
## LIBRARIES: libraries you create in this project that dependent projects also need
## CATKIN_DEPENDS: catkin_packages dependent projects also need
## DEPENDS: system dependencies of this project that dependent projects also need
catkin_package(
  INCLUDE_DIRS include
#  LIBRARIES bart_ros
#  CATKIN_DEPENDS roscpp std_msgs visualization_msgs
#  DEPENDS system_lib
)

###########
## Build ##
###########

## Specify additional locations of header files
## Your package locations should be listed before other locations
include_directories(include)
include_directories(
  ${catkin_INCLUDE_DIRS}
)

# Search for bart
find_package(bart)
IF (BART_FOUND)
  MESSAGE(STATUS "using bart version ${BART_VERSION} from ${BART_LIBRARY_DIRS} (${BART_LIBRARIES})")
  INCLUDE_DIRECTORIES(${BART_INCLUDE_DIRS})
ELSE (BART_FOUND)
  message(FATAL_ERROR "ERROR: Cannot find bart")
ENDIF (BART_FOUND)

find_package(OpenCV 2.4.2 REQUIRED core imgproc highgui ml contrib flann calib3d objdetect video flann features2d )

MESSAGE(STATUS "using opencv version ${OPENCV_VERSION} from ${OPENCV_PREFIX}")
INCLUDE_DIRECTORIES(${OpenCV_INCLUDE_DIRS})

SET(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" )
if ("$ENV{Boost_DIR}" STREQUAL "")
  set(BOOST_ROOT ${CMAKE_INSTALL_PREFIX})
endif()
# search for Boost (we need boost >= 1.54 in order to have boost.log)
find_package(Boost 1.54  COMPONENTS thread chrono program_options system filesystem regex log log_setup date_time)
if (Boost_FOUND)
  message(STATUS "using boost version ${Boost_VERSION} from ${Boost_LIBRARY_DIRS}")
  INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS} )
  add_definitions(-DBOOST_LOG_AVAILABLE=1)
else (Boost_FOUND)
  message(STATUS "could not find a boost version with boost log. building without boost-log support!")
  find_package(Boost 1.46 REQUIRED COMPONENTS thread chrono program_options system filesystem regex  date_time)
  if (Boost_FOUND)
    message(STATUS "using boost version ${Boost_VERSION} from ${Boost_LIBRARY_DIRS}")
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS} )
  else (Boost_FOUND)
    message(FATAL_ERROR "could not find boost")
  endif (Boost_FOUND)
endif(Boost_FOUND)

ADD_DEFINITIONS(-D BOOST_ALL_DYN_LINK)




## Declare a C++ library
# add_library(bart_ros
#   src/${PROJECT_NAME}/bart_ros.cpp
# )

## Add cmake target dependencies of the library
## as an example, code may need to be generated before libraries
## either from message generation or dynamic reconfigure
# add_dependencies(bart_ros ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})

# Declare a C++ executable
 add_executable(bart_ros_node src/bart_ros_node.cpp)

## Add cmake target dependencies of the executable
## same as for the library above
# add_dependencies(bart_ros_node ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})

## Specify libraries to link a library or executable target against
 target_link_libraries(bart_ros_node
   ${catkin_LIBRARIES}
   ${OpenCV_LIBS}
   ${Boost_LIBRARIES}
   ${Boost_LIBS}
   ${BART_LIBRARIES}
 )

#############
## Install ##
#############

# all install targets should use catkin DESTINATION variables
# See http://ros.org/doc/api/catkin/html/adv_user_guide/variables.html

## Mark executable scripts (Python etc.) for installation
## in contrast to setup.py, you can choose the destination
# install(PROGRAMS
#   scripts/my_python_script
#   DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
# )

# Mark executables and/or libraries for installation
install(TARGETS bart_ros_node
   ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
   LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
   RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

## Mark cpp header files for installation
# install(DIRECTORY include/${PROJECT_NAME}/
#   DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
#   FILES_MATCHING PATTERN "*.h"
#   PATTERN ".svn" EXCLUDE
# )

## Mark other files for installation (e.g. launch and bag files, etc.)
# install(FILES
#   # myfile1
#   # myfile2
#   DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
# )

#############
## Testing ##
#############

## Add gtest based cpp test target and link libraries
# catkin_add_gtest(${PROJECT_NAME}-test test/test_bart_ros.cpp)
# if(TARGET ${PROJECT_NAME}-test)
#   target_link_libraries(${PROJECT_NAME}-test ${PROJECT_NAME})
# endif()

## Add folders to be run by python nosetests
# catkin_add_nosetests(test)

#workaround for qtcreator ide integration. do not remove!
file(GLOB_RECURSE NODE_DUMMY_TARGETS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h *.cfg *.yaml *.xml *.launch)
add_custom_target(_dummy_target SOURCES ${NODE_DUMMY_TARGETS})

