#!/bin/bash
# @author: Stefano fioravanzo

PROJECT_PATH="/code/"
SIM_FOLDER="${PROJECT_PATH}sim/"
RESULTS_FOLDER="${PROJECT_PATH}results/"

# aggregate_stat:
#     Average results from multiple simulations
#
# Args:
#     $1: DataCollection / SourceRouting averaging
#     $2: Number of runs of simulation
#     $3: Prefix string
aggregate_stat () {
    echo -n ${3} >> ${SIM_FOLDER}${SIMULATION}/sim_average.log

    for (( sim=1; sim<=${2}; sim++ ))
    do
        grep "Overall PDR" ${SIM_FOLDER}${SIMULATION}/${sim}/stat_res.log | \
        sed -n ${1}p | \
        egrep -o "[0-9]+\.[0-9]+"
    done | \
    paste -sd+ - | \
    awk -v N=${2} '{print "("$1")/"N}' | \
    bc | \
    awk '{print " "$1"%"}' >> ${SIM_FOLDER}${SIMULATION}/sim_average.log

}

# run_simulation:
#     Run N simulation with different random seeds
#
# Args:
#     $1: Simulation dir path. Location of result log files
#     $2: Number of runs of simulation
#     $3: csc file name
run_simulation () {
    echo "Root folder "${1}
    for (( i=1; i<=${2}; i++ ))
    do
    	echo "Experiment "${i}
    	mkdir -p ${SIM_FOLDER}${1}/${i}
    	cooja_nogui ${SIM_FOLDER}$3 | tee ${SIM_FOLDER}${1}/${i}/cooja_output.log
    	mv ./*.log ${SIM_FOLDER}${1}/${i}
        mv ./*.testlog ${SIM_FOLDER}${1}/${i}
    	python3 ${SIM_FOLDER}parse-stats.py ${SIM_FOLDER}${1}/${i}/test.log > ${SIM_FOLDER}${1}/${i}/stat_res.log
        mv ${SIM_FOLDER}recv.csv ${SIM_FOLDER}${1}/${i}
        mv ${SIM_FOLDER}sent.csv ${SIM_FOLDER}${1}/${i}
    done
}

# prepare_env:
#    Copy the firmware (app.sky) to an appropriate location to run the cooja simulation
prepare_env () {
    mkdir -p ${SIM_FOLDER}src
    cp ${PROJECT_PATH}src/app.sky ${SIM_FOLDER}src/app.sky
    # copy the csc file to appropriate location
    cp ${SIM_FOLDER}cooja/${CSC_FILE} ${SIM_FOLDER}
}

# destroy_env:
#     Clean the project compile files and remove firmware from sim folder
destroy_env () {
    rm ${SIM_FOLDER}${CSC_FILE}
    rm -rf ${SIM_FOLDER}src
}

print_help () {
    echo "Wireless Sensors Netowrks Project"
    echo "-- Stefano Fioravanzo"
    echo
    echo "Commands:"
    echo -e "\t-r|--run-simulation\tRun simulation flag. If not set just run results aggregation"
    echo -e "\t-s|--simulation\t\tName of the simulation"
    echo -e "\t-n|--number\t\tNumber of simulations to run"
    echo -e "\t-c|--csc-file\t\tCooja csc config file. Default test_nogui_dc.csc"
    echo -e "\t-h|--help\t\tPrint this help and exit"

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

# copy the app.sky to sim/src/folder
prepare_env

if [[ $RUN -eq "1" ]]; then
    echo "RUN the simulation"
    rm -rf ${SIM_FOLDER}${SIMULATION}
    run_simulation $SIMULATION $NUM_SIMS $CSC_FILE
fi

# Average simulations results
rm ${SIM_FOLDER}${SIMULATION}/sim_average.log
echo "Simulation ${SIM_FOLDER}${SIMULATION} summary:" >> ${SIM_FOLDER}${SIMULATION}/sim_average.log
aggregate_stat 1 ${NUM_SIMS} "Data Collection PDR: "
aggregate_stat 2 ${NUM_SIMS} "Source Routing PDR: "

# Copy the test csc cooja file into the simulation folder for reproducibility
cp ${SIM_FOLDER}${CSC_FILE} ${SIM_FOLDER}${SIMULATION}/${CSC_FILE}

# Show summary
cat ${SIM_FOLDER}${SIMULATION}/sim_average.log

# move $SIMULATION folder to results/
mkdir -p ${RESULTS_FOLDER}
mv ${SIM_FOLDER}${SIMULATION}/ ${RESULTS_FOLDER}

# clean up
destroy_env
