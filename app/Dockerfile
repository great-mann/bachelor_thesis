FROM ubuntu:20.04

# Prevent tzdata prompt
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC
# Install dependencies for OpenCV build
RUN apt-get update && apt-get install -y \
    build-essential cmake git wget unzip pkg-config \
    libgtk2.0-dev libavcodec-dev libavformat-dev libswscale-dev \
    libv4l-dev libxvidcore-dev libx264-dev libjpeg-dev libpng-dev libtiff-dev \
    libopenexr-dev libtbb2 libtbb-dev libdc1394-22-dev \
    libboost-all-dev libyaml-cpp-dev 

# Build OpenCV from source
WORKDIR /opt
RUN git clone --branch 4.5.5 https://github.com/opencv/opencv.git && \
    git clone --branch 4.5.5 https://github.com/opencv/opencv_contrib.git && \
    mkdir -p opencv/build && cd opencv/build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/usr/local \
          -DOPENCV_EXTRA_MODULES_PATH=/opt/opencv_contrib/modules \
          -DBUILD_EXAMPLES=OFF .. && \
    make -j$(nproc) && make install

# Set OpenCV_DIR so CMake can find it
ENV OpenCV_DIR=/usr/local/lib/cmake/opencv4
# Set up app working directory
WORKDIR /app

# Copy your source code into the container
COPY . .

# Create a build directory and run CMake
#RUN mkdir build && cd build && \
#    cmake -DBUILD_BART_JAVA=OFF .. && \
 #   make -j$(nproc)

# Set working directory to the build output
WORKDIR /app/build

# Run the compiled binary by default
#CMD ["./testMarkerTracker"]