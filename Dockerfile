# syntax=docker/dockerfile:1.4
ARG FG=euresys
FROM ubuntu:22.04 as fg_euresys

RUN --mount=from=euresys,target=/host/euresys \
	mkdir -p /opt/euresys && \
	cp -r /host/euresys/* /opt/euresys/


FROM ubuntu:22.04 as fg_stub_only

FROM fg_${FG}

ARG FG

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

RUN if [ "$FG" = "stub_only" ] ; then cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFORCE_STUB_FRAMEGRABBER_ONLY=On ..; else cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..; fi

RUN make all check install && ldconfig

RUN if [ "$FG" = "euresys" ]; then ln -s /opt/euresys/egrabber/firmware/coaxlink-firmware /usr/local/bin/coaxlink-firmware; fi

ENV GENICAM_GENTL64_PATH=/opt/euresys/egrabber/lib/x86_64
ENV EURESYS_COAXLINK_GENTL64_CTI=/opt/euresys/egrabber/lib/x86_64/coaxlink.cti
ENV EURESYS_EGRABBER_LIB64=/opt/euresys/egrabber/lib/x86_64
ENV EURESYS_DEFAULT_GENTL_PRODUCER=coaxlink

CMD [ "artemis" ]
