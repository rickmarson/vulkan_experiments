FROM nevil82/vulkan_experiments:vulkan_base_ubuntu20.04

ENV DEBIAN_FRONTEND noninteractive

RUN mkdir -p /opt/vulkan/model_viewer

ADD model_viewer.tar.gz /opt/vulkan/model_viewer

RUN chmod +x /opt/vulkan/model_viewer/model_viewer

