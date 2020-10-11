NAME=vulkan_base_ubuntu20.04
TAG=nevil82/vulkan_experiments:${NAME}

build:
	cp /usr/lib/x86_64-linux-gnu/libvulkan_intel.so ./libvulkan_intel.so
	cp /usr/lib/x86_64-linux-gnu/libvulkan_radeon.so ./libvulkan_radeon.so
	docker build --tag ${TAG} -f Dockerfile .
	rm ./libvulkan_intel.so
	rm ./libvulkan_radeon.so

publish:
	docker push ${TAG}

run-shell:
	docker run -it \
		--rm \
		--entrypoint bash \
		--net=host \
		${TAG}
