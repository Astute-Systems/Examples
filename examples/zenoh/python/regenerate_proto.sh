#!/bin/bash

mkdir -p ./proto
protoc --python_out=./proto -I../../icds/proto/ common_can.proto
protoc --python_out=./proto -I../../icds/proto/ gnss.proto
protoc --python_out=./proto -I../../icds/proto/ mit_motor.proto
protoc --python_out=./proto -I../../icds/proto/ mr_motor.proto