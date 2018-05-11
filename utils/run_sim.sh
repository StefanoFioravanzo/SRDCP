#!/bin/bash
# Copyright 2018 Stefano Fioravanzo
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# aggregate_stat:
#     Average results from multiple simulations
#
# Args:
#     $1: DataCollection / SourceRouting averaging
#     $2: Number of runs of simulation
aggregate_stat () {
    echo -n ${2} >> ${SIMULATION}/sim_average.log

    for (( sim=1; sim<=$NUM_SIMS; sim++ ))
    do
        grep "Overall PDR" ${SIMULATION}/${sim}/stat_res.log | \
        sed -n ${1}p | \
        egrep -o "[0-9]+\.[0-9]+"
    done | \
    paste -sd+ - | \
    awk -v N=$NUM_SIMS '{print "("$1")/"N}' | \
    bc | \
    awk '{print " "$1"%"}' >> ${SIMULATION}/sim_average.log

}

# run_simulation:
#     Run N simulation with different random seeds
#
# Args:
#     $1: Simulation dir path. Location of result log files
#     $2: Number of runs of simulation
#     $3: csc file name
run_simulation () {
    echo "Root folder "$1
    for (( i=1; i<=$NUM_SIMS; i++ ))
    do
    	echo "Experiment "$i
    	mkdir -p $SIMULATION/$i
    	cooja_nogui $3 > $SIMULATION/$i/cooja_output.log
    	mv ./*.log $SIMULATION/$i
    	python parse-stats.py $SIMULATION/$i/test.log > $SIMULATION/$i/stat_res.log
    done
}

print_help () {
    echo "Wireless Sensor Netowrk Project"
    echo "-- Stefano Fioravanzo"
    echo
    echo "Commands"
    echo -e "\t-s|--simulation\tRun the simulation. Default 1 simulation"
    echo -e "\t-n|--number\tNumber of simulations to run"
    echo -e "\t-c|--csc-file\tCooja csc config file. Default test_nogui_dc.csc"
    echo -e "\t-h|--help\tPrint this help and exit"

    exit 0
}

# Defaults
RUN=0
NUM_SIMS=1
SIMULATION="default"
CSC_FILE="test_nogui_dc.csc"

# Parse CLI arguments
POSITIONAL=()
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -h|--help)
    print_help
    shift
    ;; # past argument
    -s|--simulation)
    SIMULATION="$2"
    shift # past argument
    shift # past value
    ;;
    -n|--number)
    NUM_SIMS="$2"
    shift # past argument
    shift # past value
    ;;
    -r|--run-simulation)
    RUN=1
    shift # past argument
    ;;
    -c|--csc-file)
    CSC_FILE=$2
    shift # past argument
    shift # past value
    ;;
    *)    # unknown option
    POSITIONAL+=("$1") # save it in an array for later
    shift # past argument
    ;;
esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

echo Running with conf:
echo SIMULATION     = "${SIMULATION}"
echo NUMBER OF RUNS = "${NUM_SIMS}"
echo RUN            = "${RUN}"
echo CSC FILE       = "${CSC_FILE}"
echo

if [[ $RUN -eq "1" ]]; then
    echo "RUN the simulation"
    rm -rf $SIMULATION
    run_simulation $SIMULATION $NUM_SIMS $CSC_FILE
fi

# Average simulations results
rm ${SIMULATION}/sim_average.log
echo "Simulation ${SIMULATION} summary:" >> ${SIMULATION}/sim_average.log
aggregate_stat 1 "Data Collection PDR: "
aggregate_stat 2 "Source Routing PDR: "

# Show summary
cat ${SIMULATION}/sim_average.log
