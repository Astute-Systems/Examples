#!/bin/bash

mkdir -p ./python
protoc --python_out=./python -I. common_can.proto
