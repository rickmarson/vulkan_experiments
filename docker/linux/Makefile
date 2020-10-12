VERSION=1.0.0
TAG=nevil82/vulkan_experiments:${VERSION}

build:
	mkdir -p ../../out
	cd ../../out; \
		cmake -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" ..; \
		make -j4

build-debug:
	mkdir -p ../../out
	cd ../../out; \
		cmake -DCMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" ..; \
		make -j4

package:
	tar -C ../../out/apps -zcvf model_viewer.tar.gz model_viewer
	docker build --tag ${TAG} -f Dockerfile .
	rm model_viewer.tar.gz

publish:
	docker push ${TAG}

run-vulkaninfo:
	docker run -it --runtime=nvidia --rm \
	  	--entrypoint=vulkaninfo \
		--net=host \
		-v /usr/share/vulkan:/usr/share/vulkan \
		-v /tmp/.X11-unix:/tmp/.X11-unix \
		-e DISPLAY=${DISPLAY}  \
		--privileged \
		${TAG}

run-model-viewer:
	docker run -it --runtime=nvidia --rm \
	  	--entrypoint=/opt/vulkan/model_viewer/model_viewer \
		--net=host \
		-v /usr/share/vulkan:/usr/share/vulkan \
		-v /tmp/.X11-unix:/tmp/.X11-unix \
		-e DISPLAY=${DISPLAY}  \
		--privileged \
		${TAG}

run-shell:
	docker run -it --rm \
		--entrypoint bash \
		--net=host \
		${TAG}

