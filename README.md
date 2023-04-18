OpenDTAM for Jetson Nano
========

An open source implementation of DTAM

Based on Newcombe, Richard A., Steven J. Lovegrove, and Andrew J. Davison's "DTAM: Dense tracking and mapping in real-time."

This project depends on qtbase5-dev, [OpenCV 3](https://github.com/Itseez/opencv "OpenCV") and [Cuda](https://developer.nvidia.com/cuda-downloads "Cuda").

## Changes
The main contributions over TeddyBearCrisis's fork are:

- Adaptation to the Jetson Nano SDK: A docker image built on the 32.7.1 L4T-ML container is provided. This allows one to build DTAM on top of OpenCV 4.5.0 (built with CUDA). The CMake file has been updated to generate code for the right CUDA arch (5.3).

- Updated CUDA and OpenCV code: Cuda code has been updated to compile under toolkit 10.2. This mainly involved replacing \_\_shfl\_up() and \_\_shfl\_down() ops with the newer \_\_shfl\_up\_sync() and \_\_shfl\_down\_sync(). The OpenCV code for 3.1 used outdated CV\_XXX flags.

The build has been tested on a Jetson Nano JetPack SDK running 32.7.1 L4T. The base hardware was a 2GB Developer Kit. The Nano was run in headless mode using VNC Viewer to display the GUI on a laptop. You may find instructions on the Nano setup on the NVIDIA page "Getting Started with Jetson Nano 2GB Developer Kit" under "Next Steps", then "Setting up VNC".

## Build Instructions on JetPack's Ubuntu 18.04

Tested in this environment

* Ubuntu 18.04 x64
* GCC 7.5.0
* Boost 1.5.8
* OpenCV 4.5 (supplied by the L4T-ML container)
* Cuda Toolkit 10.2 (supplied by JetPack)

### Install dependencies

The following have all been accomplished in the provided Dockerfile. Nevertheless, for local builds, you must take note of some changes.

#### qtbase5-dev

~~`sudo apt-add-repository ppa:ubuntu-sdk-team/ppa`~~ [^1]
```bash
sudo apt-get update
sudo apt-get install qtbase5-dev
```

[^1]: Ubuntu-SDK has no release for 18.04. The build worked okay without it.

#### boost

```bash
sudo apt-get install libboost-system-dev libboost-thread-dev
```

#### Cuda

Version 10.2 was used. 

~~You can use the pre-built downloads from [NVIDIA](https://developer.nvidia.com/cuda-downloads "Cuda"), or you can follow this guide:~~

~~[Cuda Installation Tutorial](https://www.pugetsystems.com/labs/hpc/NVIDIA-CUDA-with-Ubuntu-16-04-beta-on-a-laptop-if-you-just-cannot-wait-775/ "Cuda Installation Tutorial")~~

Nothing to be done here if you are using L4T. No instructions provided here for building cudatoolkit without a container. The NVIDIA Forum might have some posts that can help.

#### OpenCV 4

~~These lines were mostly stitched together from the [caffee installation guide](https://github.com/BVLC/caffe/wiki/Ubuntu-16.04-or-15.10-OpenCV-3.1-Installation-Guide "caffe installation guide")~~

Again, the L4T ML container is already built with OpenCV + CUDA support.

For local builds, try github.com/mdegans/nano\_build\_opencv.

Old OpenCV Build Instructions:

```bash
# Execute first command from directory you would like to clone OpenCV
git clone https://github.com/opencv/opencv.git
cd opencv
# make sure to use version 3.1.0
git checkout tags/3.1.0
mkdir build
cd build
cmake -D CMAKE_BUILD_TYPE=RELEASE -D CMAKE_INSTALL_PREFIX=/usr/local -D WITH_TBB=ON -D WITH_V4L=ON -D WITH_QT=ON -D WITH_OPENGL=ON -DCUDA_NVCC_FLAGS="-D_FORCE_INLINES" ..
make -j4
sudo make install
```

### Build OpenDTAM

Old DTAM Build Instructions:
```bash
cd OpenDTAM
mkdir build
cd build
cmake ../Cpp
make -j4
````

### Run OpenDTAM
The code has been tested on the original Trajectory_30_seconds dataset. These files were deleted for the purposes of our build. They inflated the Docker context to 1.81 GB. Without the folder, the context was 0.8 GB. If you need the dataset, please retrieve it from TeddyBearCrisis' fork or any previous fork. You can then mount a volume with the dataset upon running the container.

#### Build the container:
First move the Dockerfile to the parent directory of OpenDTAM-3.1.
`mv Dockerfile ..`

Then, run:
`sudo docker build -t dtam_ml .`

#### Run the container:

`sudo docker run -it --env="DISPLAY" --env="QT_X11_NO_MITSHM=1" --volume="/tmp/.X11-unix:/tmp/.X11-unix:rw" -v <your local dataset directory>:<dataset path in container fs> dtam_ml`

Explanation:

- Run in interactive mode
- Add the environmental variable DISPLAY and QT...
- Mount the X11 server into the container
- Mount the dataset directory for testing

Your shell will now run as root@ubuntu, the root user of the container.

#### Allow X11 streaming from the container
In a separate terminal, run:

```bash
export containerId=$(sudo docker ps -l -q)
xhost +local:`sudo docker inspect --format='{{.Config.Hostname}}' $containerId`
```

Explanation:
The container needs to be able to stream to the GUI on the local machine. We are giving permission to the container instance to be able to do so. It may be wise to run the same 'xhost' command with '-local' afterwards to strip this permission, for security reasons.

#### Run DTAM
For the following command, replace `$TRAJECTORY_30_SECONDS` with the path to the directory of the same name.
```bash
./a.out $TRAJECTORY_30_SECONDS
```
Assuming you are executing this command from the build folder, as shown above, enter the following:
```bash
./a.out ../Trajectory_30_seconds
```
