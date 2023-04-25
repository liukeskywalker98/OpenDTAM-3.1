FROM nvcr.io/nvidia/l4t-ml:r32.7.1-py3

# Dependencies from the default Ubuntu repos
RUN apt-get update
RUN apt-get install -y --no-install-recommends software-properties-common
RUN apt-get update
#RUN apt-add-repository ppa:ubuntu-sdk-team/ppa
RUN apt-get update && apt-get install -y --no-install-recommends \
	git \
	vim \
	libboost-program-options-dev \
	libboost-filesystem-dev \
	libboost-graph-dev \
	libboost-system-dev \
	libboost-test-dev \
	libboost-thread-dev\
	qtbase5-dev
	#nvidia-cuda-toolkit \
	#nvidia-cuda-toolkit-gcc

# Install OpenDTAM
COPY OpenDTAM-3.1 /usr/dtam/OpenDTAM-3.1
WORKDIR /usr/dtam/OpenDTAM-3.1
RUN mkdir build && \
	cd build && \
	cmake ../Cpp && \
	make -j4
#	cmake -D CMAKE_BUILD_TYPE=RELEASE -D CMAKE_INSTALL_PREFIX=/usr/local -D WITH_TBB=ON -D WITH_V4L=ON -D WITH_QT=ON -D WITH_OPENGL=ON -DCUDA_NVCC_FLAGS="-D_FORCE_INLINES" ..
#	make -j4
#	sudo make install

#COPY .git .git
# Enable wget by updating ca-certificates
# install cmake 3.20 from tar
#RUN apt-get install -y --reinstall ca-certificates && \
#	wget https://cmake.org/files/v3.20/cmake-3.20.0-linux-aarch64.tar.gz && \
#	tar zxvf cmake-3.20.0-linux-aarch64.tar.gz && \
#	mv cmake-3.20.0-linux-aarch64 /opt/cmake-3.20.0-linux-aarch64 && \
#	ln -sf /opt/cmake-3.20.0-linux-aarch64/bin/* /usr/bin/  

# Install COLMAP
#COPY colmap_build/colmap colmap
#RUN cd colmap && \
#	mkdir build && \
#	cd build &&\
#	cmake .. -GNinja -DCMAKE_CUDA_ARCHITECTURES="53" && \
#	ninja && \
#	ninja install

# Install PYCOLMAP
#COPY pycolmap_build/pycolmap pycolmap
#RUN cd pycolmap && \
#	apt-get install python3-pip -y && \
#	pip3 install . && \
#	cd ../..

# Install Jetcam
#COPY camera/jetcam jetcam
#RUN cd jetcam && \
#	python3 setup.py install
