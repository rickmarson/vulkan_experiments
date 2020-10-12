NAME=vulkan_base_ubuntu20.04
TAG=nevil82/vulkan_experiments:${NAME}

build:
	cp /usr/lib/x86_64-linux-gnu/libvulkan_intel.so ./libvulkan_intel.so.x64
	cp /usr/lib/x86_64-linux-gnu/libvulkan_radeon.so ./libvulkan_radeon.so.x64
	cp /usr/lib/i386-linux-gnu/libvulkan_intel.so ./libvulkan_intel.so.x86
	cp /usr/lib/i386-linux-gnu/libvulkan_radeon.so ./libvulkan_radeon.so.x86
	docker build --tag ${TAG} -f Dockerfile .
	rm -f ./libvulkan_intel.so.x64 ./libvulkan_radeon.so.x64 ./libvulkan_intel.so.x86 ./libvulkan_radeon.so.x86

publish:
	docker push ${TAG}

run-shell:
	docker run -it \
		--rm \
		--entrypoint bash \
		--net=host \
		${TAG}

run-vulkaninfo:
	docker run -it --runtime=nvidia --rm \
	  	--entrypoint=vulkaninfo \
		--net=host \
		-v /usr/share/vulkan:/usr/share/vulkan \
		-v /tmp/.X11-unix:/tmp/.X11-unix \
		-e DISPLAY=${DISPLAY}  \
		--privileged \
		${TAG}

run-dev-env:
	docker run -it --runtime=nvidia --rm \
	  	--entrypoint=bash \
		--net=host \
		-v /usr/share/vulkan:/usr/share/vulkan \
		-v /tmp/.X11-unix:/tmp/.X11-unix \
		-v ~/repos:/home/vulkan/repos \
		-e DISPLAY=${DISPLAY}  \
		--privileged \
		${TAG}
