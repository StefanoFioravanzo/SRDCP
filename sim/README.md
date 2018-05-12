## Running Simulations

Cooja is the Contiki network simulator. Cooja allows large and small networks of Contiki motes to be simulated. Motes can be emulated at the hardware level, which is slower but allows precise inspection of the system behavior, or at a less detailed level, which is faster and allows simulation of larger networks.

Every simulation is described using a `csc` file, which is just an `xml` specification of the motes network. Every aspect of the simulation configuration can be set using the `csc` file. 

TODO: Explain more the csc file. Find a way to pass parameters at runtime.

--

##### `run_sim.sh`

This script automatically runs `n` simulations and produces a final log file with the average performance of the simulations. For example the command

```bash
./run_sim.sh -r -s test_sim -n 4 -c cooja/test_no_gui.csc
```

will run four different simulation (with four different random seeds) using the configuration found in the `cooja/test_no_gui.csc` file. All the cooja logs and resulting files will be places under a new `test_sim` folder.

Under the hood, `run_sim.sh` calls the script `parse-stats.py` at each run. This python script reads the `test.log` output file (look at the end of `test_no_gui.csc` for an example of how this file is produced) and aggregated all results in a few summary metrics of packet delivery. It also creates `recv.csv` and `sent.csv` listing all the packets sent and received during the simulation in an easy to read format.

## EMPIRICAL RESUTLS:

Analyze PDR varying these parameters:

BEACON_INTERVAL
Clock second * N
Topology report hold time
BEACON_FORWARD_DELAY
TOPOLOGY_REPORT_INTERVAL
