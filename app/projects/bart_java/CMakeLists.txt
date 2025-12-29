MESSAGE(STATUS "=======================================================")
MESSAGE(STATUS "=======================================================")
MESSAGE(STATUS "==================JAVA BART CMAKE BEGIN================")
MESSAGE(STATUS "=======================================================")
MESSAGE(STATUS "=======================================================")

SET( PROJECT_NAME bart_java )
PROJECT( ${PROJECT_NAME} )
CMAKE_MINIMUM_REQUIRED(VERSION 3.4)



#debug?
set(CMAKE_BUILD_TYPE Debug)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Test 32/64 bits
if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
	message(STATUS "Target is 64 bits")
	if (WIN32)
		SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/x64/Debug")
		SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/x64/Release")
		SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/x64/Debug")
		SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/x64/Release")
		SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/x64/Debug")
		SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/x64/Release")
	endif(WIN32)
else("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
	message(STATUS "Target is 32 bits")
	if (WIN32)
		SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/x86/Debug")
		SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/x86/Release")
		SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/x86/Debug")
		SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/x86/Release")
		SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/x86/Debug")
		SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/x86/Release")
	endif(WIN32)
endif("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fstack-protector")

SET(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" )

set (Boost_DEBUG TRUE)

find_package(Java REQUIRED)
find_package(JNI REQUIRED)
include(UseJava)

if (JNI_FOUND)
    message (STATUS "JNI_INCLUDE_DIRS=${JNI_INCLUDE_DIRS}")
    message (STATUS "JNI_LIBRARIES=${JNI_LIBRARIES}")
endif()

# Search for bart
#find_package(bart)
#IF (BART_FOUND)
#  MESSAGE(STATUS "using bart version ${BART_VERSION} from ${BART_LIBRARY_DIRS}")
#  INCLUDE_DIRECTORIES(${BART_INCLUDE_DIRS})
##ELSE (BART_FOUND)
#  message(FATAL_ERROR "ERROR: Cannot find bart")
#ENDIF (BART_FOUND)

set (Boost_DEBUG TRUE)

# search for Boost (we need boost >= 1.54 in order to have boost.log)
find_package(Boost 1.54  COMPONENTS thread program_options system filesystem regex log log_setup date_time)
if (Boost_FOUND)
  message(STATUS "using boost version ${Boost_VERSION} from ${Boost_LIBRARY_DIRS}")
  INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS} )
  add_definitions(-DBOOST_LOG_AVAILABLE=1)
else (Boost_FOUND)
  message(STATUS "could not find a boost version with boost log. building without boost-log support!")
  find_package(Boost 1.46 REQUIRED COMPONENTS thread program_options system filesystem regex  date_time)
  if (Boost_FOUND)
    message(STATUS "using boost version ${Boost_VERSION} from ${Boost_LIBRARY_DIRS}")
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS} )
  else (Boost_FOUND)
    message(FATAL_ERROR "could not find boost")
  endif (Boost_FOUND)
endif(Boost_FOUND)

ADD_DEFINITIONS(-D BOOST_ALL_DYN_LINK)

find_package(OpenCV 2.4.9 REQUIRED core imgproc highgui ml contrib flann calib3d objdetect video flann features2d )
MESSAGE(STATUS "using opencv version ${OpenCV_VERSION} from ${OpenCV_INSTALL_PATH}")
INCLUDE_DIRECTORIES(${OpenCV_INCLUDE_DIRS})
LINK_DIRECTORIES(${OpenCV_LIBRARY_DIRS} ${Boost_LIBRARY_DIRS} ${LIBDIR})

#
# The JAVA classes
#
# We are adding a dependency on opencv for java. Not keeping care of installing
# the required dll..

#set(CMAKE_JAVA_COMPILE_FLAGS "-source" "1.6" "-target" "1.6")
set(CMAKE_JAVA_INCLUDE_PATH ${OpenCV_INSTALL_PATH}/share/OpenCV/java/opencv-249.jar;${OpenCV_LIB_DIR}/java/opencv-249.jar)

IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
	set(BART_JavaInterface_Install_Dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG})
ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)

IF(CMAKE_BUILD_TYPE MATCHES RELEASE)
	set(BART_JavaInterface_Install_Dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE})
ENDIF(CMAKE_BUILD_TYPE MATCHES RELEASE)


#	No longer needed, because of own manifest: ENTRY_POINT de.citec.clfvr.bart.MarkerTracker
add_jar(BART_JavaInterface
	src/main/java/de/citec/clfvr/bart/MarkerTracker.java
	src/main/java/de/citec/clfvr/bart/TestMarkerTracker.java
	OUTPUT_DIR ${BART_JavaInterface_Install_Dir}
	MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/MANIFEST.MF
	INCLUDE_JARS ${OpenCV_INSTALL_PATH}/share/OpenCV/java/opencv-249.jar;opencv-249.jar;${OpenCV_LIB_DIR}/java/opencv-249.jar
)
get_target_property(_jarFile BART_JavaInterface JAR_FILE)
set_source_files_properties(${_jarFile} PROPERTIES GENERATED TRUE) 
get_target_property(_classDir BART_JavaInterface CLASSDIR)
#
# The JNI Interface
#

# Generating the Header file using javah
#
set (_stubDir "${CMAKE_CURRENT_BINARY_DIR}")
#MESSAGE(STATUS "Classdir is ${_classDir}")
#add_custom_command(
#    OUTPUT de_citec_clfvr_bart_MarkerTracker.h
#    COMMAND ${Java_JAVAH_EXECUTABLE} -verbose
#        -classpath ${_classDir}
#        -d ${_stubDir}
#        -jni de.citec.clfvr.bart.MarkerTracker
#    )

find_path(OpenCV_JAR NAMES opencv-249.jar PATHS ${OpenCV_INSTALL_PATH}/share/OpenCV/java/;${OpenCV_LIB_DIR}/java/opencv-249.jar)

MESSAGE(STATUS "OpenCV Jar is ${OpenCV_JAR}/opencv-249.jar")

#${OpenCV_JAR}/opencv-249.jar;
set(javah_classpath "${OpenCV_JAR}/opencv-249.jar")
MESSAGE( STATUS "JAVAH: using CLASSPATH ${javah_classpath}")
Create_javah(GENERATED_FILES files_headers
  CLASSES de.citec.clfvr.bart.MarkerTracker
  CLASSPATH BART_JavaInterface ${javah_classpath}
  DEPENDS BART_JavaInterface
)

#
# Compiling the JNI library stub
#
# generate libfoo.jnilib
include_directories(${JAVA_INCLUDE_PATH} ${JAVA_INCLUDE_PATH2} ${BART_INCLUDE_DIRS} ${_classDir} ${_stubDir})
include_directories(${PROJECT_SOURCE_DIR}/include)
add_library(BART_JNI MODULE src/c/JNI_MarkerTracker.cc de_citec_clfvr_bart_MarkerTracker.h)
# This did not work on windows...
#set_target_properties(BART_JNI PROPERTIES SUFFIX ".jnilib")
target_link_libraries(BART_JNI bart ${JNI_LIBRARIES} ${BART_LIBRARIES} ${Boost_LIBRARIES} ${OpenCV_LIBS})

MESSAGE(STATUS "=======================================================")
MESSAGE(STATUS "=======================================================")
MESSAGE(STATUS "=================JAVA BART CMAKE END===================")
MESSAGE(STATUS "=======================================================")
MESSAGE(STATUS "=======================================================")

