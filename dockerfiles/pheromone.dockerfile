FROM cheneyyu/faas_base:latest

ENV PHERO_HOME /pheromone
USER root

WORKDIR /
# COPY . /pheromone
RUN git clone https://github.com/Tingjia980311/pheromone.git

WORKDIR $PHERO_HOME
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install libopencv-dev python-opencv --assume-yes
RUN apt-get install -y yasm
RUN bash scripts/build.sh -g -bRelease

WORKDIR /
RUN cp /pheromone/dockerfiles/start-pheromone.sh /
CMD bash start-pheromone.sh
