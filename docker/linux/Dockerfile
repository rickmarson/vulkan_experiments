FROM nevil82/vulkan_experiments:vulkan_base_ubuntu20.04

ENV DEBIAN_FRONTEND noninteractive

RUN sudo mkdir -p /opt/vulkan/

ADD model_viewer.tar.gz /opt/vulkan/
ADD rainy_alley.tar.gz /opt/vulkan/

RUN chmod +x /opt/vulkan/model_viewer/model_viewer
