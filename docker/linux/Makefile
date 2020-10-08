VERSION=1.0.0
TAG=nevil82/vulkan_experiments:${VERSION}

build:
	docker build --tag ${TAG} -f Dockerfile
	# docker build --tag ${TAG} - < Dockerfile

publish:
	docker push ${TAG}

run-vulkan:
	docker run -it --runtime=nvidia --rm \
	  	--entrypoint bash \
		--net=host \
		-v /tmp/.X11-unix:/tmp/.X11-unix  -e DISPLAY=${DISPLAY}  \
		${TAG} \
	    "/opt/ningaloo/start_vulkan.sh"
