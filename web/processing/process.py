#!/usr/env/python
import sys
import re
import subprocess

data_dir = sys.argv[1]
results_dir = data_dir + "/results"
outfile = sys.argv[2]
target_cpus = sys.argv[3]

print "Processing data in %s" % data_dir
print "Target CPUs are: %s" % target_cpus.split(",")

processor_ids = []
model_names = []
for line in open(data_dir + "/logs/cpuinfo").readlines():
  r = re.search("processor\t: ([0-9]+)", line)
  if r:
    processor_ids.append(r.group(1))
  r = re.search("model name\t: (.+)", line)
  if r:
    model_names.append(r.group(1))

num_cores = len(processor_ids)

# uname/OS
os_string = "unknown"
for line in open(data_dir + "/logs/uname").readlines():
  fields = line.split()
  os = fields[0]
  ver = fields[1]
  arch = fields[2]
  os_string = "%s %s, %s" % (os, ver, arch)

# Generate graphs
argv = ["python", "plot_thr.py", results_dir, target_cpus, "0"]
#argv.extend(str(target_cpus))
subprocess.check_call(argv)

# Generate HTML output
html = "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td></tr>"

thr_graphs_links = ""
for c in target_cpus.split(","):
  graph_file = data_dir + "/graphs/core_0_to_%s.png" % c
  thr_graphs_links = thr_graphs_links + "<a href=\"%s\">0 to %s</a> " \
      % (graph_file, c)

#out_html = html % (num_cores, "<br />".join(model_names), os_string,
out_html = html % (num_cores, model_names[0], os_string,
                   thr_graphs_links)

out_fd = open(outfile, "a")
out_fd.write(out_html)
out_fd.close()
