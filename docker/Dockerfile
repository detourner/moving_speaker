#FROM python:3.11-bullseye
FROM python:3.14.7

RUN apt-get update && apt-get install -y \
    git \
    build-essential

RUN pip install platformio==6.1.19 intelhex

CMD ["/bin/bash"]