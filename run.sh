#!/bin/bash
cd ./app-linux

if [ "$1" == "-b" ] ; then
    ./aicup22 --config ../config.json --save-results ./res.json --batch-mode
else
  ./aicup22 --config ../config.json
fi
