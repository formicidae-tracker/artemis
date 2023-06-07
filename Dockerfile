FROM ubuntu:22.04 as build

RUN --mount=from=euresys,target=/host/euresys \
	mkdir -p /opt/euresys && \
	cp -r /host/euresys/* /opt/euresys/

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

RUN cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..

RUN make all check install && ldconfig

RUN ldd /usr/local/bin/artemis | cut -d " " -f 3 > artemis_libs.txt

FROM ubuntu:22.04

COPY --from=build /opt/euresys/egrabber/firmware/coaxlink-firmware /usr/local/bin/

COPY --from=build /opt/euresys/egrabber/lib/x86_64 /opt/euresys/egrabber/lib/x86_64

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

ENV GENICAM_GENTL64_PATH=/opt/euresys/egrabber/lib/x86_64
ENV EURESYS_COAXLINK_GENTL64_CTI=/opt/euresys/egrabber/lib/x86_64/coaxlink.cti
ENV EURESYS_EGRABBER_LIB64=/opt/euresys/egrabber/lib/x86_64
ENV EURESYS_DEFAULT_GENTL_PRODUCER=coaxlink

RUN groupadd -g 1000 fort-user
RUN useradd -d /home/fort-user -s /bin/sh -m fort-user -u 1000 -g 1000
USER fort-user
ENV HOME /home/fort-user

CMD [ "artemis" ]
