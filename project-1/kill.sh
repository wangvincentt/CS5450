#!/bin/bash -  
kill -9 $(lsof -i tcp:9990| awk '{print $2}')