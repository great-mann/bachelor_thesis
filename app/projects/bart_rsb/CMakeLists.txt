CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
INCLUDE(FindPkgConfig)

SET( PROJECT_NAME bart_rsb )
PROJECT( ${PROJECT_NAME} )

SET(BART_RSB_LIBNAME "${PROJECT_NAME}")
SET(BART_RSB_VERSION_MAJOR    "0")
SET(BART_RSB_VERSION_MINOR    "1")
SET(BART_RSB_VERSION_REVISION "0")
SET(BART_RSB_VERSION   "${BART_RSB_VERSION_MAJOR}.${BART_RSB_VERSION_MINOR}.${BART_RSB_VERSION_REVISION}")
SET(BART_RSB_SOVERSION "${BART_RSB_VERSION_MAJOR}.${BART_RSB_VERSION_MINOR}")
SET(BART_RSB_PACKAGE   "${BART_RSB_LIBNAME}-${BART_RSB_VERSION_MAJOR}.${BART_RSB_VERSION_MINOR}")

SET(PREFIX      ${CMAKE_INSTALL_PREFIX})
SET(BINDIR     "${PREFIX}/bin")
SET(INCLUDEDIR "${PREFIX}/include/bart_rsb")
SET(MANDIR     "${PREFIX}/man")
SET(LIBDIR     "${PREFIX}/lib")
SET(PKGDIR     "${PREFIX}/lib/pkgconfig")
SET(CFGDIR     "${PREFIX}/etc/bart_rsb")
SET(DATADIR    "${PREFIX}/share/bart_rsb")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fstack-protector")

SET(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" )
if ("$ENV{Boost_DIR}" STREQUAL "")
  set(BOOST_ROOT ${CMAKE_INSTALL_PREFIX})
endif()


#set (Boost_DEBUG TRUE)

# search for Boost (we need boost >= 1.54 in order to have boost.log)
find_package(Boost 1.54  COMPONENTS chrono thread program_options system filesystem regex log log_setup date_time)
if (Boost_FOUND)
  message(STATUS "using boost version ${Boost_VERSION} from ${Boost_LIBRARY_DIRS}")
  INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS} )
  add_definitions(-DBOOST_LOG_AVAILABLE=1)
else (Boost_FOUND)
  message(STATUS "could not find a boost version with boost log. building without boost-log support!")
  find_package(Boost 1.46 REQUIRED COMPONENTS chrono thread program_options system filesystem regex  date_time)
  if (Boost_FOUND)
    message(STATUS "using boost version ${Boost_VERSION} from ${Boost_LIBRARY_DIRS}")
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS} )
  else (Boost_FOUND)
    message(FATAL_ERROR "could not find boost")
  endif (Boost_FOUND)
endif(Boost_FOUND)

ADD_DEFINITIONS(-D BOOST_ALL_DYN_LINK)

# Search for opencv
find_package(OpenCV 2.4.2 REQUIRED core imgproc highgui ml contrib flann calib3d objdetect video flann features2d )
MESSAGE(STATUS "using opencv version ${OPENCV_VERSION} from ${OPENCV_PREFIX}")
INCLUDE_DIRECTORIES(${OpenCV_INCLUDE_DIRS})

# Search for RST/RSC/RSB

#RSB
SET(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib:\$ORIGIN/")
FIND_PACKAGE(RSC 0.11 REQUIRED)
FIND_PACKAGE(RSB 0.11 REQUIRED)
find_package(RSC 0.11 REQUIRED)
find_package(rst-converters 0.11 REQUIRED)
#RST
FIND_PACKAGE(RST REQUIRED COMPONENTS sandbox)
INCLUDE_DIRECTORIES(BEFORE SYSTEM ${RST_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(BEFORE SYSTEM ${RST_CONVERTERS_INCLUDE_DIRS})
ADD_DEFINITIONS(${RST_CFLAGS} ${RSTSANDBOX_CFLAGS})

INCLUDE_DIRECTORIES(BEFORE SYSTEM ${RSB_INCLUDE_DIRS})
LIST(INSERT CMAKE_MODULE_PATH 0 ${RSC_CMAKE_MODULE_PATH})


# Search for bart
find_package(bart)
IF (BART_FOUND)
  MESSAGE(STATUS "using bart version ${BART_VERSION} from ${BART_LIBRARY_DIRS}")
  INCLUDE_DIRECTORIES(${BART_INCLUDE_DIRS})
ELSE (BART_FOUND)
  message(FATAL_ERROR "ERROR: Cannot find bart")
ENDIF (BART_FOUND)


# Look for the message files...
set(PROTO_CPP_DIR "${PROJECT_SOURCE_DIR}/external/cpp" CACHE PATH "Directory of the proto files for sending tracking information")
MESSAGE(STATUS ${PROTO_CPP_DIR})
IF(EXISTS ${PROTO_CPP_DIR})
    INCLUDE_DIRECTORIES(${PROTO_CPP_DIR}/include)
    set(PROTO_CPP ${PROTO_CPP_DIR}/src/Orientation.pb.cc
                  ${PROTO_CPP_DIR}/src/Matrix.pb.cc
                  ${PROTO_CPP_DIR}/src/Position.pb.cc
                  ${PROTO_CPP_DIR}/src/TransformEvent.pb.cc
                  ${PROTO_CPP_DIR}/src/Transform.pb.cc  )
ELSE (EXISTS ${PROTO_CPP_DIR})
  message(FATAL_ERROR "ERROR: Cannot find tracking proto cpps! Set PROTO_CPP_DIR to the correct directory.")
ENDIF (EXISTS ${PROTO_CPP_DIR})


#this dummy list will make all header files appear if you use qtcreator to open the cmake project
file(GLOB HEADER_LIST RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} include/*.h ${PROTO_CPP_DIR}/include/*.h )

#add_library(${BART_RSB_LIBNAME} SHARED
#	src/bart_rsb.cc
#	${HEADER_LIST}
#)
#
#target_link_libraries(${BART_RSB_LIBNAME} ${Boost_LIBRARIES} ${OpenCV_LIBS})
#SET_TARGET_PROPERTIES(${BART_RSB_LIBNAME} PROPERTIES VERSION ${BART_RSB_VERSION} SOVERSION ${BART_RSB_SOVERSION})


#build & install bart_rsb
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)
LINK_DIRECTORIES(${OpenCV_LIBRARY_DIRS} ${Boost_LIBRARY_DIRS} ${RSB_LIBRARY_DIRS} ${RST_LIBRARY_DIRS} ${RSC_LIBRARY_DIRS} ${RST_CONVERTERS_LIBRARY_DIRS} ${LIBDIR})
ADD_EXECUTABLE(bart_rsb src/bart_rsb.cc ${PROTO_CPP} ${HEADER_LIST})
TARGET_LINK_LIBRARIES(bart_rsb ${OpenCV_LIBS} ${Boost_LIBRARIES} ${Boost_LIBS} ${RSB_LIBRARIES} ${RST_LIBRARIES} ${RSC_LIBRARIES} ${RST_CONVERTERS_LIBRARIES} ${BART_LIBRARIES})
install(TARGETS bart_rsb DESTINATION bin)



#INSTALL(FILES ${HEADER_LIST} DESTINATION ${INCLUDEDIR})


#create package config file
SET(PKGCONFIG_FILENAME "bart_rsb.pc")
configure_file("${PKGCONFIG_FILENAME}.in" "${PKGCONFIG_FILENAME}" @ONLY)
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PKGCONFIG_FILENAME}" DESTINATION "${PKGDIR}")

#create cmake config files
SET(CMAKECONFIG_FILENAME "bart_rsbConfig.cmake")
configure_file("${CMAKECONFIG_FILENAME}.in" "${CMAKECONFIG_FILENAME}" @ONLY)
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/${CMAKECONFIG_FILENAME}" DESTINATION "${DATADIR}")
SET(CMAKECONFIGVERSION_FILENAME "bart_rsbConfigVersion.cmake")
configure_file("${CMAKECONFIGVERSION_FILENAME}.in" "${CMAKECONFIGVERSION_FILENAME}" @ONLY)
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/${CMAKECONFIGVERSION_FILENAME}" DESTINATION "${DATADIR}")


