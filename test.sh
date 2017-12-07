#!/bin/bash

docker build -t mixtest -f Dockerfile.test .

docker run -d --rm \
-it \
--mount type=bind,source="$(pwd)",target=/home/digital_signature.api mixtest:latest
