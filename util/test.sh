#!/bin/bash

PATH=`pwd`/work/nginx/sbin:$PATH TEST_NGINX_NO_SHUFFLE=1 prove -I../test-nginx/lib -r t
