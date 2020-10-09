VERSION=1.0.0
TAG=nevil82/vulkan_experiments:${VERSION}

build:
	mkdir -p ../../out
	cd ../../out; \
		cmake -G "Unix Makefiles" ..; \
		make -j4
	tar -zcvf model_viewer.tar.gz ../../out/apps/model_viewer

package:
	docker build --tag ${TAG} - < Dockerfile

publish:
	docker push ${TAG}

run-model-viewer:
	docker run -it --runtime=nvidia --rm \
	  	--entrypoint bash \
		--net=host \
		-v /tmp/.X11-unix:/tmp/.X11-unix  -e DISPLAY=${DISPLAY}  \
		${TAG} \
	    "/opt/vulkan/model_viewer/model_viewer"
