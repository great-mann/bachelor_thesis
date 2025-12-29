CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
INCLUDE(FindPkgConfig)

SET( PROJECT_NAME bart_iio )
PROJECT( ${PROJECT_NAME} )

SET(BART_INSTANTIO_LIBNAME "${PROJECT_NAME}")
SET(BART_INSTANTIO_VERSION_MAJOR    "0")
SET(BART_INSTANTIO_VERSION_MINOR    "1")
SET(BART_INSTANTIO_VERSION_REVISION "0")
SET(BART_INSTANTIO_VERSION   "${BART_INSTANTIO_VERSION_MAJOR}.${BART_INSTANTIO_VERSION_MINOR}.${BART_INSTANTIO_VERSION_REVISION}")
SET(BART_INSTANTIO_SOVERSION "${BART_INSTANTIO_VERSION_MAJOR}.${BART_INSTANTIO_VERSION_MINOR}")
SET(BART_INSTANTIO_PACKAGE   "${BART_INSTANTIO_LIBNAME}-${BART_INSTANTIO_VERSION_MAJOR}.${BART_INSTANTIO_VERSION_MINOR}")

SET(PREFIX      ${CMAKE_INSTALL_PREFIX})
SET(BINDIR     "${PREFIX}/bin")
SET(INCLUDEDIR "${PREFIX}/include/bart_iio")
SET(MANDIR     "${PREFIX}/man")
SET(LIBDIR     "${PREFIX}/lib")
SET(PKGDIR     "${PREFIX}/lib/pkgconfig")
SET(CFGDIR     "${PREFIX}/etc/bart_iio")
SET(DATADIR    "${PREFIX}/share/bart_iio")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fstack-protector")

SET(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" )
if ("$ENV{Boost_DIR}" STREQUAL "")
  set(BOOST_ROOT ${CMAKE_INSTALL_PREFIX})
endif()


#set (Boost_DEBUG TRUE)

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

# Search for opencv

find_package(OpenCV 2.4.2 REQUIRED core imgproc highgui ml contrib flann calib3d objdetect video flann features2d )

  MESSAGE(STATUS "using opencv version ${OPENCV_VERSION} from ${OPENCV_PREFIX}")
  INCLUDE_DIRECTORIES(${OpenCV_INCLUDE_DIRS})



# set platform specific paths for Instant Reality
if(WIN32)
   set(IR_ROOT "C:/Programme/Instant Reality" CACHE PATH "Path to InstantReality")
   set(IR_HEADERS "${IR_ROOT}/Include")
   set(IR_LIBS "${IR_ROOT}/lib/InstantIO.lib")
   set(IR_BIN "${IR_ROOT}/bin")
   if(EXISTS ${IR_LIBS})
       set(IR_FOUND TRUE)
   endif()
else()
    set(IR_ROOT "/vol/clfvr/opt" CACHE PATH "Path to InstantReality")
    set(IR_HEADERS "${IR_ROOT}/include")
    set(IR_LIBS "${IR_ROOT}/lib/libInstantIO.so")
    if(EXISTS ${IR_LIBS})
       set(IR_FOUND TRUE)
   endif()
endif()
if (NOT IR_FOUND)
    message(FATAL_ERROR "ERROR: Cannot find InstantReality!")
endif()


# adds the InstantIO lib and include dir to search paths
include_directories(${IR_HEADERS})
link_directories(${IR_LIBS})


# Search for bart
find_package(bart)
IF (BART_FOUND)
  MESSAGE(STATUS "using bart version ${BART_VERSION} from ${BART_LIBRARY_DIRS} (${BART_LIBRARIES})")
  INCLUDE_DIRECTORIES(${BART_INCLUDE_DIRS})
ELSE (BART_FOUND)
  message(FATAL_ERROR "ERROR: Cannot find bart")
ENDIF (BART_FOUND)


#this dummy list will make all header files appear if you use qtcreator to open the cmake project
file(GLOB HEADER_LIST RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} include/*.h  )

if(WIN32)
	add_definitions(-DInstantIO_EXPORTS)
endif()
add_library(${BART_INSTANTIO_LIBNAME} SHARED
	src/bart_iio.cc
	${HEADER_LIST}
)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)
LINK_DIRECTORIES(${OpenCV_LIBRARY_DIRS} ${Boost_LIBRARY_DIRS} ${IR_LIBS} ${BART_LIBRARY_DIRS} ${LIBDIR})
target_link_libraries(${BART_INSTANTIO_LIBNAME} ${Boost_LIBRARIES} ${IR_LIBS} ${OpenCV_LIBS} ${BART_LIBRARIES})
SET_TARGET_PROPERTIES(${BART_INSTANTIO_LIBNAME} PROPERTIES VERSION ${BART_INSTANTIO_VERSION} SOVERSION ${BART_INSTANTIO_SOVERSION})


#build & install bart_iio
#ADD_EXECUTABLE(bart_iio src/bart_iio.cc ${HEADER_LIST})
#TARGET_LINK_LIBRARIES(bart_iio ${OpenCV_LIBS} ${Boost_LIBRARIES} ${Boost_LIBS} bart)
#install(TARGETS bart_iio DESTINATION bin)


install (TARGETS bart_iio DESTINATION lib )
INSTALL(FILES ${HEADER_LIST} DESTINATION ${INCLUDEDIR})
set_target_properties(${BART_INSTANTIO_LIBNAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
set_target_properties(${BART_INSTANTIO_LIBNAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
set_property(TARGET ${BART_INSTANTIO_LIBNAME} PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)


#create package config file
SET(PKGCONFIG_FILENAME "bart_iio.pc")
configure_file("${PKGCONFIG_FILENAME}.in" "${PKGCONFIG_FILENAME}" @ONLY)
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PKGCONFIG_FILENAME}" DESTINATION "${PKGDIR}")

#create cmake config files
SET(CMAKECONFIG_FILENAME "bart_iioConfig.cmake")
configure_file("${CMAKECONFIG_FILENAME}.in" "${CMAKECONFIG_FILENAME}" @ONLY)
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/${CMAKECONFIG_FILENAME}" DESTINATION "${DATADIR}")
SET(CMAKECONFIGVERSION_FILENAME "bart_iioConfigVersion.cmake")
configure_file("${CMAKECONFIGVERSION_FILENAME}.in" "${CMAKECONFIGVERSION_FILENAME}" @ONLY)
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/${CMAKECONFIGVERSION_FILENAME}" DESTINATION "${DATADIR}")


