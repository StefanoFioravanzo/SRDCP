#!/bin/bash
# $1: path to csc file
cat $1 | grep -E "<y>|<x>|<id>" | sed -e "s/<x>/{'x':/g" -e "s/<y>/'y':/g" -e "s/<id>/'id':/g" -e "s/<\/x>/,/g" -e "s/<\/y>/,/g" -e "s/<\/id>/},/g" | tr -d ' '
