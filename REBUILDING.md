There are significant version-to-version differences in various tools used in the build process.
Here we document how to build the firmware and the software using a relatively new (as of 2026)
toolchain.

OS: Ubuntu 24.04 LTS, updated on 28 Mar 2026, kernel 6.17.0-19-generic

Submodules: PulsePins depends on the stand-alone asio library. After cloning the repository,
you can run: git submodule update --init

Firmware rebuild:
Run make in project root directory.

The ARM software needs to be compiled using a toolchain with compatible ABI. That is not easy to
do under Ubuntu 24.04, even using gcc cross-compilers (gcc-14-arm-linux-gnueabihf-base
g++-14-arm-linux-gnueabihf), due to glibc incompatibilities. The easiest approach is to set up
Ubuntu 20.04 in a container, as follows:

sudo apt update
sudo apt install -y software-properties-common
sudo add-apt-repository -y ppa:apptainer/ppa
sudo apt update
sudo apt install -y apptainer

mkdir -p ~/containers
cd ~/containers

apptainer build --sandbox ubuntu2004-gcc10 docker://ubuntu:20.04

Inside the container:

apt update
apt install -y g++
apt install -y \
  gcc-10-arm-linux-gnueabihf \
  g++-10-arm-linux-gnueabihf \
  binutils-arm-linux-gnueabihf \
  libc6-dev-armhf-cross \
  pkg-config \
  make \
  cmake \
  git

mkdir /work

Then run as:

apptainer shell --writable --fakeroot   --bind $HOME/$PULSEPINSROOT:/work   ubuntu2004-gcc10

cd $PULSEPINSROOT/c++
make
