#!/bin/bash

rm -f *.xz && makepkg -s && rm -rf pkg src terminol
