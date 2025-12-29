MESSAGE(STATUS "=======================================================")
MESSAGE(STATUS "=======================================================")
MESSAGE(STATUS "===================BART CMAKE BEGIN====================")
MESSAGE(STATUS "========================INCLUDE(GenerateExportHeader)===============================")
MESSAGE(STATUS "=======================================================")
CMAKE_MINIMUM_REQUIRED(VERSION 3.25.1)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



# for windows development:
OPTION(BUILD_BART_JAVA "build bart_java or not" OFF)
OPTION(BUILD_BART_RSB_SERVER "build bart_rsb_server" OFF)

set( PROJECT_NAME bart )
PROJECT( ${PROJECT_NAME} )
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a+simd")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math -funroll-loops -mcpu=cortex-a72")

endif()


set(BART_LIBNAME "${PROJECT_NAME}")
set(BART_VERSION_MAJOR    "0")
set(BART_VERSION_MINOR    "1")
set(BART_VERSION_REVISION "0")
set(BART_VERSION   "${BART_VERSION_MAJOR}.${BART_VERSION_MINOR}.${BART_VERSION_REVISION}")
set(BART_SOVERSION "${BART_VERSION_MAJOR}.${BART_VERSION_MINOR}")
set(BART_PACKAGE   "${BART_LIBNAME}-${BART_VERSION_MAJOR}.${BART_VERSION_MINOR}")

include(GNUInstallDirs)

set(INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/bart") #headers
set(DATADIR    "${CMAKE_INSTALL_DATADIR}/bart") #data (ex.png)
set(CFGDIR     "${CMAKE_INSTALL_SYSCONFDIR}/bart") #config files (ex. config.yaml)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules")

set (Boost_DEBUG TRUE)
# --- Find dependencies ---
find_package(Threads REQUIRED)

find_package(Boost 1.70 REQUIRED COMPONENTS
  log log_setup
  thread filesystem program_options system regex date_time
)

find_package(yaml-cpp REQUIRED)

find_package(OpenCV REQUIRED COMPONENTS aruco core imgproc highgui calib3d videoio video)

message(STATUS "Using Boost ${Boost_VERSION}")
message(STATUS "Using OpenCV ${OpenCV_VERSION} (config at: ${OpenCV_DIR})")


option(BUILD_SHARED_LIBS "Build libraries as shared" ON)

# --- Define the library FIRST ---
add_library(${BART_LIBNAME}
  src/Anytime.cc
  src/CameraCalibration.cc
  src/Camera.cc
  src/DebugHelpers.cc
  src/GeometryHelpers.cc
  src/MarkerBody.cc
  src/Marker.cc
  src/MarkerTrackerConfig.cc
  src/MedianFlow.cc
  src/Stopwatch.cc
  src/MultiMarkerTracker.cc
  src/HypothesisManager.cc
  src/MarkerTracker.cc
  src/KalmanMarker.cc
  src/KalmanMarkerList.cc
  src/DetectionLegacy.cpp
  ${HEADER_LIST}
)

# Public/Private include paths
target_include_directories(${BART_LIBNAME}
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compile defs for optional features
target_compile_definitions(${BART_LIBNAME} PRIVATE
  BOOST_LOG_AVAILABLE=1
  YAML_CPP_AVAILABLE=1
)

# Link deps with imported targets (modern)
target_link_libraries(${BART_LIBNAME} PRIVATE
  Boost::log Boost::log_setup
  Boost::thread Boost::filesystem Boost::program_options
  Boost::system Boost::regex Boost::date_time
  Threads::Threads
  yaml-cpp
  opencv_aruco
  opencv_core
  opencv_imgproc
  opencv_highgui
  opencv_calib3d
  opencv_videoio
  opencv_video 
)

set_target_properties(${BART_LIBNAME} PROPERTIES
  VERSION   ${BART_VERSION}
  SOVERSION ${BART_SOVERSION}
  INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}"
  INSTALL_RPATH_USE_LINK_PATH TRUE
)

# --- Executables ---
# ---add_executable(testMarkerTracker example/testMarkerTracker.cc)
# ---target_link_libraries(testMarkerTracker PRIVATE
 # --- bart
 # --- Threads::Threads
 # --- opencv_core opencv_imgproc opencv_highgui opencv_calib3d opencv_videoio opencv_video opencv_aruco
 # --- Boost::thread
 # --- Boost::program_options
 # --- Boost::system
# ---)

add_executable(optimizedMarkerTracker example/optimizedMarkerTracker.cpp)
target_link_libraries(optimizedMarkerTracker PRIVATE
  bart
  Threads::Threads
  opencv_core opencv_imgproc opencv_highgui opencv_calib3d opencv_videoio opencv_video opencv_aruco
  Boost::thread
  Boost::program_options
  Boost::system
)
target_compile_options(optimizedMarkerTracker PRIVATE 
    -O3 
    -march=armv8-a+simd 
    -fopenmp
)
target_link_libraries(optimizedMarkerTracker PRIVATE gomp) # Link against the OpenMP library
add_executable(benchmark_SAT_NEON example/benchmark_SAT_NEON.cpp)
target_link_libraries(benchmark_SAT_NEON PRIVATE
  bart
  Threads::Threads
  opencv_core opencv_imgproc opencv_highgui opencv_calib3d opencv_videoio opencv_video opencv_aruco
  Boost::thread
  Boost::program_options
  Boost::system
)
add_executable(benchmark_OPENCV_ADAPTIVE example/benchmark_OPENCV_ADAPTIVE.cpp)
target_link_libraries(benchmark_OPENCV_ADAPTIVE PRIVATE
  bart
  Threads::Threads
  opencv_core opencv_imgproc opencv_highgui opencv_calib3d opencv_videoio opencv_video opencv_aruco
  Boost::thread
  Boost::program_options
  Boost::system
)
add_executable(minimalMarkerTracker example/minimalMarkerTracker.cc)
target_link_libraries(minimalMarkerTracker PRIVATE
  bart
  Threads::Threads
  opencv_core opencv_imgproc opencv_highgui opencv_calib3d opencv_videoio opencv_video opencv_aruco
  Boost::thread
  Boost::program_options
  Boost::system
)

add_executable(cameraCalibration projects/cameraCalibration/src/CameraCalibration.cc)
target_link_libraries(cameraCalibration PRIVATE
  bart
  Threads::Threads
  opencv_core opencv_imgproc opencv_highgui opencv_calib3d opencv_videoio opencv_video opencv_aruco
  Boost::thread
  Boost::program_options
  Boost::system
)

# --- Install ---
# ---install(TARGETS testMarkerTracker minimalMarkerTracker cameraCalibration
 # ---       RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS bart
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${HEADER_LIST} DESTINATION ${INCLUDEDIR})

install(DIRECTORY data/
        DESTINATION ${DATADIR}/data
        FILES_MATCHING PATTERN "*.png")

# pkg-config
set(PKGCONFIG_FILENAME "bart.pc")
configure_file("${PKGCONFIG_FILENAME}.in" "${PKGCONFIG_FILENAME}" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PKGCONFIG_FILENAME}"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# CMake package files
set(CMAKECONFIG_FILENAME "bartConfig.cmake")
configure_file("${CMAKECONFIG_FILENAME}.in" "${CMAKECONFIG_FILENAME}" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${CMAKECONFIG_FILENAME}"
        DESTINATION "${DATADIR}")

set(CMAKECONFIGVERSION_FILENAME "bartConfigVersion.cmake")
configure_file("${CMAKECONFIGVERSION_FILENAME}.in" "${CMAKECONFIGVERSION_FILENAME}" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${CMAKECONFIGVERSION_FILENAME}"
        DESTINATION "${DATADIR}")
message(STATUS "=======================================================")
message(STATUS "=======================================================")
message(STATUS "===================BART CMAKE END======================")
message(STATUS "=======================================================")
message(STATUS "=======================================================")


