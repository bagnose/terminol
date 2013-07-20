#!/bin/bash

rm -f *.xz && makepkg --source && makepkg && rm -rf pkg src terminol
