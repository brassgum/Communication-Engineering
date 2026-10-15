#!/bin/bash

find ./out -name "*.csv" | xargs -P 3 -I{} python3 Homework-01/save_fig.py {}

