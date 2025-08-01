# Examples

## Setup protobuf

``` .bash
pip install protobuf
```

Create the code:

``` .bash
mkdir ./proto
protoc --python_out=./proto -I../../icds/proto/ common_can.proto
protoc --python_out=./proto -I../../icds/proto/ gnss.proto
protoc --python_out=./proto -I../../icds/proto/ mit_motor.proto
```

## Troubleshooting

Unmet dependencies for python3 installer using apt:

``` .bash
sudo apt install python3-googleapi 
sudo apt install python3-protobuf 
```

``` .bash
python3 -m venv ~/zenoh
source ~/zenoh/bin/activate
(zenoh) pip3 install googleapi
(zenoh) pip3 install protobuf
(zenoh) pip3 install eclipse-zenoh
```
