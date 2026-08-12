#FROM python:3.11-bullseye
FROM python

RUN apt-get update && apt-get install -y \
    git \
    build-essential

#RUN pip install platformio
RUN pip install platformio intelhex

CMD ["/bin/bash"]