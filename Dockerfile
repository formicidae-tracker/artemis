FROM ubuntu:22.04 as build

ARG DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y \
	build-essential \
	cmake \
	git \
	git-lfs \
	protobuf-compiler \
	libprotobuf-dev \
	libboost-system-dev \
	libopencv-dev \
	libopencv-imgproc-dev \
	libasio-dev \
	libglew-dev \
	libglfw3-dev \
	libeigen3-dev \
	libfontconfig1-dev \
	libfreetype6-dev \
	libgoogle-glog-dev \
	google-mock

COPY --from=golang:1.20-bullseye /usr/local/go /usr/local/go

ENV PATH="/usr/local/go/bin:${PATH}"

WORKDIR /app/artemis

COPY . .

RUN mkdir -p build

WORKDIR /app/artemis/build

RUN cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFORCE_STUB_FRAMEGRABBER_ONLY=On ..

RUN make all check install && ldconfig

RUN ldd /usr/local/bin/artemis | cut -d " " -f 3 > artemis_libs.txt

FROM ubuntu:22.04

RUN apt update && apt install -y \
	libgoogle-glog0v5 \
	libprotobuf23 \
	libopencv-imgcodecs4.5d \
	libtbb12 \
	libglfw3 \
	libglew2.2 \
	libopengl0

COPY --from=build /usr/local/lib/libfort* /usr/local/lib/

COPY --from=build /usr/local/bin/artemis /usr/local/bin/artemis

RUN ldconfig

CMD [ "/usr/local/bin/artemis" ]
