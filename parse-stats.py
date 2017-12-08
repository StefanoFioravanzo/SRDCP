#!/usr/bin/env python2.7

from __future__ import division

import re
import sys
import os.path

sink_id = 1

def parse_file(log_file):
	# Create CSV output files
	frecv = open("recv.csv", 'w')
	fsent = open("sent.csv", 'w')

	# Write CSV headers
	frecv.write("time\tdest\tsrc\tseqn\thops\n")
	fsent.write("time\tdest\tsrc\tseqn\n")

	# Regular expressions
	record_pattern = "(?P<time>[\w:.]+)\s+ID:(?P<self_id>\d+)\s+%s"
	regex_node = re.compile(record_pattern%"Rime started with address (?P<src1>\d+).(?P<src2>\d+)")
	regex_recv = re.compile(record_pattern%"App: Recv from (?P<src1>\w+):(?P<src2>\w+) seqn (?P<seqn>\d+) hops (?P<hops>\d+)")
	regex_sent = re.compile(record_pattern%"App: Send seqn (?P<seqn>\d+)")
	regex_srrecv = re.compile(record_pattern%"App: sr_recv from sink seqn (?P<seqn>\d+) hops (?P<hops>\d+) node metric (?P<metric>\d+)")
	regex_srsent = re.compile(record_pattern%"App: sink sending seqn (?P<seqn>\d+) to (?P<dest1>\w+):(?P<dest2>\w+)")

	# Node list and dictionaries for later processing
	nodes = []
	num_resets = 0
	drecv = {}
	dsent = {}
	dsrrecv = {}
	dsrsent = {}
	
	# Parse log file and add data to CSV files
	with open(log_file, 'r') as f:
		for line in f:
			# Node boot
			m = regex_node.match(line)
			if m:
				# Get dictionary with data
				d = m.groupdict()
				node_id = int(d["self_id"])

				# Save data in the nodes list
				if node_id not in nodes:
					nodes.append(node_id)
				else:
					num_resets += 1
					print "WARNING: node {} reset during the simulation.".format(node_id)

				# Continue with the following line
				continue

			# RECV 
			m = regex_recv.match(line)
			if m:
				# Get dictionary with data
				d = m.groupdict()
				ts = d["time"]
				src = int(d["src1"], 16) # Discard second byte, and convert to decimal
				dest = int(d["self_id"])
				seqn = int(d["seqn"])
				hops = int(d["hops"])

				# Write to CSV file
				frecv.write("{}\t{}\t{}\t{}\t{}\n".format(ts, dest, src, seqn, hops))

				# Save data in the drecv dictionary for later processing
				if dest == sink_id:
					drecv.setdefault(src, {})[seqn] = ts

				# Continue with the following line
				continue

			# SENT
			m = regex_sent.match(line)
			if m:
				d = m.groupdict()
				ts = d["time"]
				src = int(d["self_id"])
				dest = 1
				seqn = int(d["seqn"])

				# Write to CSV file
				fsent.write("{}\t{}\t{}\t{}\n".format(ts, dest, src, seqn))

				# Save regex_sent data in the dsent dictionary
				dsent.setdefault(src, {})[seqn] = ts

				# Continue with the following line
				continue

			# Source Routing RECV
			m = regex_srrecv.match(line)
			if m:
				d = m.groupdict()
				ts = d["time"]
				src = 1
				dest = int(d["self_id"])
				seqn = int(d["seqn"])

				# Save RECV data in the dsent dictionary
				dsrrecv.setdefault(dest, {})[seqn] = ts

				# Continue with the following line
				continue

			# Source Routing SENT
			m = regex_srsent.match(line)
			if m:
				d = m.groupdict()
				ts = d["time"]
				src = int(d["self_id"])
				dest = int(d["dest1"], 16) # Discard second byte, and convert to decimal
				seqn = int(d["seqn"])

				# Save RECV data in the dsent dictionary
				dsrsent.setdefault(dest, {})[seqn] = ts


	# Analyze dictionaries and print some stats
	# Overall number of packets sent / received
	tsent = 0
	trecv = 0
	tsrsent = 0
	tsrrecv = 0

	if num_resets > 0:
		print "----- WARNING -----"
		print "{} nodes reset during the simulation".format(num_resets)
		print "" # To separate clearly from the following set of prints

	# Nodes that did not manage to send data
	fails = []
	for node_id in sorted(nodes):
		if node_id == sink_id:
			continue
		if node_id not in dsent.keys():
			fails.append(node_id)
	if fails:
		print "----- Data Collection WARNING -----"
		for node_id in fails:
			print "Warning: node {} did not send any data.".format(node_id)
		print "" # To separate clearly from the following set of prints

	# Print node stats
	print "----- Data Collection Node Statistics -----"
	for node in sorted(dsent.keys()):
		nsent = len(dsent[node])
		nrecv = 0
		if node in drecv.keys():
			nrecv = len(drecv[node])

		pdr = 100 * nrecv / nsent
		print "Node {}: TX Packets = {}, RX Packets = {}, PDR = {:.2f}%, PLR = {:.2f}%".format(
			node, nsent, nrecv, pdr, 100 - pdr)

		# Update overall packets sent / received
		tsent += nsent
		trecv += nrecv

	# Print overall stats
	if tsent > 0:
		print "\n----- Data Collection Overall Statistics -----"
		print "Total Number of Packets Sent: {}".format(tsent)
		print "Total Number of Packets Received: {}".format(trecv)
		opdr = 100 * trecv / tsent
		print "Overall PDR = {:.2f}%".format(opdr)
		print "Overall PLR = {:.2f}%".format(100 - opdr)

	# Print node stats
	print "\n----- Source Routing Node Statistics -----"
	for node in sorted(dsrsent.keys()):
		nsent = len(dsrsent[node])
		nrecv = 0
		if node in dsrrecv.keys():
			nrecv = len(dsrrecv[node])

		pdr = 100 * nrecv / nsent
		print "Node {}: TX Packets = {}, RX Packets = {}, PDR = {:.2f}%, PLR = {:.2f}%".format(
			node, nsent, nrecv, pdr, 100 - pdr)

		# Update overall packets sent / received
		tsrsent += nsent
		tsrrecv += nrecv

	# Print overall stats
	if tsrsent > 0:
		print "\n----- Source Routing Overall Statistics -----"
		print "Total Number of Packets Sent: {}".format(tsrsent)
		print "Total Number of Packets Received: {}".format(tsrrecv)
		opdr = 100 * tsrrecv / tsrsent
		print "Overall PDR = {:.2f}%".format(opdr)
		print "Overall PLR = {:.2f}%".format(100 - opdr)


if __name__ == '__main__':

	# Get the log file to parse and check that it exists
	log_file = sys.argv[1]
	if not os.path.isfile(log_file) or not os.path.exists(log_file):
		print "Error: No such file."
		sys.exit(1)
	
	# Parse log file, create CSV files, and print some stats
	parse_file(log_file)
