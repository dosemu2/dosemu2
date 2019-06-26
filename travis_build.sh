#!/bin/bash

./default-configure -d && make && nosetests -v test/test_dos.py
