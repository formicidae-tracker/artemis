FROM ubuntu:24.04 AS build

ARG DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y \
	build-essential \
	cmake \
	git \
	git-lfs \
	protobuf-compiler \
	libprotobuf-dev \
	libasio-dev \
	libglew-dev \
	libglfw3-dev \
	libeigen3-dev \
	libfontconfig1-dev \
	libfreetype6-dev \
	libgstreamer-plugins-base1.0-dev


WORKDIR /app/artemis

COPY . .

RUN mkdir -p build

WORKDIR /app/artemis/build

RUN cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFORCE_STUB_FRAMEGRABBER_ONLY=On ..

RUN make -j 16 all install && ldconfig

RUN ldd /usr/local/bin/artemis | cut -d " " -f 3 > artemis_libs.txt


FROM ubuntu:24.04

RUN apt update && apt install -y \
	libprotobuf32 \
	libglfw3 \
	libglew2.2 \
	libopengl0 \
	libunwind8 \
	gstreamer1.0-plugins-base \
	gstreamer1.0-plugins-bad \
	intel-media-va-driver-non-free


COPY --from=build /usr/local/lib/libfort* /usr/local/lib/libspng* /usr/local/lib/

COPY --from=build /usr/local/bin/artemis /usr/local/bin/artemis

COPY --from=build /app/artemis/build/src/artemis-tests /usr/local/bin

RUN ldconfig

WORKDIR /app

CMD [ "/usr/local/bin/artemis" ]
