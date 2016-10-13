FROM httpd

RUN apt-get update
RUN apt-get install -y git cmake pkg-config vim 
RUN apt-get install -y apache2-dev libapr1-dev libaprutil1-dev
