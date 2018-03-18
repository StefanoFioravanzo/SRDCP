#!/bin/bash

ROOT=$1
echo "Root folder "$1
for i in {1..4}
do
	echo "Experiment "$i
	mkdir -p $ROOT$i
	cooja_nogui test_nogui_dc.csc > $ROOT$i/cooja_output.log
	mv ./*.log $ROOT$i
	python parse-stats.py $ROOT$i/test.log > $ROOT$i/stat_res.log
done
