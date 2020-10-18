#!/bin/bash

reset
echo "compiling..."
g++ -std=c++11 -g -Wall -Wshadow -DDEBUG -lusb-1.0 -pthread usb_app.cpp -o usb_app
echo "compile done"
