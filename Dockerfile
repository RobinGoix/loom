FROM ubuntu:20.04
ARG GRB_VERSION=9.1.2
ARG GRB_SHORT_VERSION=9.1

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install --no-install-recommends -y\
       ca-certificates  \
	   libglpk-dev \
	   glpk-utils \
	   coinor-libcbc-dev \
	   coinor-cbc \
	   libprotobuf-dev \
	   libzip-dev \
	   protobuf-compiler \
       make \
       cmake \
       wget \
	   g++ \
       git \
    && update-ca-certificates

RUN wget --no-check-certificate https://packages.gurobi.com/9.1/gurobi9.1.2_linux64.tar.gz
RUN tar -xf gurobi${GRB_VERSION}_linux64.tar.gz  \
    && rm -f gurobi${GRB_VERSION}_linux64.tar.gz \
    && mv -f gurobi* gurobi \
    && rm -rf gurobi/linux64/docs

ENV GUROBI_HOME /opt/gurobi/linux64
ENV PATH $PATH:$GUROBI_HOME/bin:/loom/build
ENV LD_LIBRARY_PATH $GUROBI_HOME/lib

RUN git clone --recurse-submodules https://github.com/ad-freiburg/loom /loom

ENV GRB_LICENSE_FILE /gurobi/gurobi.lic

RUN cd /loom && rm -rf build
RUN cd /loom && mkdir build 
RUN cd /loom/build && cmake .. && make install

WORKDIR /

CMD loom
