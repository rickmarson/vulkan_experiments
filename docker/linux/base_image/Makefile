NAME=vulkan_base_ubuntu20.04
TAG=nevil82/vulkan_experiments:${NAME}

build:
	docker build --tag ${TAG} - < Dockerfile

publish:
	docker push ${TAG}

run-shell:
	docker run -it -
		--rm 
		--entrypoint bash \
		--net=host \
		${TAG} 
