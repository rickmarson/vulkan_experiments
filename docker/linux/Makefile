VERSION=1.0.0
TAG=nevil82/vulkan_experiments:${VERSION}

build:
	mkdir -p ../../out
	cd ../../out; \
		cmake -G "Unix Makefiles" ..; \
		make -j4
	tar -C ../../out/apps -zcvf model_viewer.tar.gz model_viewer

package:
	docker build --tag ${TAG} -f Dockerfile .

publish:
	docker push ${TAG}

run-model-viewer:
	docker run -it --runtime=nvidia --rm \
	  	--entrypoint /opt/vulkan/model_viewer/model_viewer \
		--net=host \
		-v /tmp/.X11-unix:/tmp/.X11-unix  -e DISPLAY=${DISPLAY}  \
		${TAG}

run-shell:
	docker run -it --rm \
		--entrypoint bash \
		--net=host \
		${TAG}

