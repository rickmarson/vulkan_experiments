FROM nvidia/cudagl:11.0-devel-ubuntu20.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update \
  && apt-get -y install sudo \
  && apt-get upgrade -y \
  && apt-get install -y build-essential git unzip curl wget cmake

RUN wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add - \
  && wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.2.148-focal.list https://packages.lunarg.com/vulkan/1.2.148/lunarg-vulkan-1.2.148-focal.list

RUN apt-get update \
  && apt-get install -y \
      libgl1 \
      libgl1-mesa-glx \
      libgl1-mesa-dev \
      x11proto-core-dev \
      libx11-dev \
      libxrandr-dev \
      libxinerama-dev \
      libxcursor-dev \
      libxi-dev \
      vulkan-sdk

COPY libvulkan_intel.so.x64 /usr/lib/x86_64-linux-gnu/libvulkan_intel.so
COPY libvulkan_radeon.so.x64 /usr/lib/x86_64-linux-gnu/libvulkan_radeon.so
COPY libvulkan_intel.so.x86 /usr/lib/i386-linux-gnu/libvulkan_intel.so
COPY libvulkan_radeon.so.x86 /usr/lib/i386-linux-gnu/libvulkan_radeon.so

RUN useradd -m vulkan && echo "vulkan:vulkan" | chpasswd && adduser vulkan sudo
RUN echo "vulkan ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

USER vulkan
