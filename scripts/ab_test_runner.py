import os
import sys
import subprocess
from io import StringIO
import csv
import statistics

# how many times we will run the experiment, to be sure of the regression
number_repetitions = 1
# the threshold at which we consider something a regression (percentage)
regression_threshold_percentage = 0.01
# minimal seconds diff for something to be a regression (for very fast benchmarks)
regression_threshold_seconds = 0.00002

old_runner = None
new_runner = None
benchmark_file = None
verbose = False
threads = None
for arg in sys.argv:
    if arg.startswith("--old="):
        old_runner = arg.replace("--old=", "")
    elif arg.startswith("--new="):
        new_runner = arg.replace("--new=", "")
    elif arg.startswith("--benchmarks="):
        benchmark_file = arg.replace("--benchmarks=", "")
    elif arg == "--verbose":
        verbose = True
    elif arg.startswith("--threads="):
        threads = int(arg.replace("--threads=", ""))

if old_runner is None or new_runner is None or benchmark_file is None:
    print("Expected usage: python3 scripts/regression_test_runner.py --old=/old/benchmark_runner --new=/new/benchmark_runner --benchmarks=/benchmark/list.csv")
    exit(1)

if not os.path.isfile(old_runner):
    print(f"Failed to find old runner {old_runner}")
    exit(1)

if not os.path.isfile(new_runner):
    print(f"Failed to find new runner {new_runner}")
    exit(1)

def run_benchmark(A, benchmark, old_timings):
    benchmark_args = [A, benchmark]
    if threads is not None:
        benchmark_args += ["--threads=%d" % (threads,)]
    proc = subprocess.Popen(benchmark_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out = proc.stdout.read().decode('utf8')
    err = proc.stderr.read().decode('utf8')
    proc.wait()
    if proc.returncode != 0:
        print("Failed to run benchmark " + benchmark)
        print('''====================================================
==============         STDERR          =============
====================================================
''')
        print(err)
        print('''====================================================
==============         STDOUT          =============
====================================================
''')
        print(out)
        exit(1)
    if verbose:
        print(err)
    # read the input CSV
    f = StringIO(err)
    csv_reader = csv.reader(f, delimiter='\t')
    header = True
    timings = old_timings
    try:
        for row in csv_reader:
            if len(row) == 0:
                continue
            if header:
                header = False
            else:
                timings.append(float(row[2]))
    except:
        print("Failed to run benchmark " + benchmark)
        print(err)
        exit(1)
    return timings

def run_benchmark_both(A, B, benchmark):
    timeA = []
    timeB = []
    estimate = 1.0
    n = 1.0
    while abs(estimate) > 1.0*n/10 and n < 10.5:
        if n%2:
            timeA = run_benchmark(A, benchmark, timeA)
            timeB = run_benchmark(B, benchmark, timeB)
        else:
            timeB = run_benchmark(B, benchmark, timeB)
            timeA = run_benchmark(A, benchmark, timeA)
        meanA = float(statistics.mean(timeA))
        stdevA = float(statistics.stdev(timeA))
        meanB = float(statistics.mean(timeB))
        stdevB = float(statistics.stdev(timeB))
        distA = statistics.NormalDist(meanA, stdevA)
        distB = statistics.NormalDist(meanB, stdevB)
        meanR = meanA - meanB
        stdevR = pow(stdevA*stdevA + stdevB*stdevB, 0.5)
        over = distA.overlap(distB)
        estimate = meanR / stdevR
        n = n+1
        if verbose:
            print(benchmark, meanR / stdevR)
    return {'res': meanR / stdevR, 'stddev': stdevR, 'meanA': meanA, 'meanB': meanB}

def run_benchmarks(A, B, benchmark_list):
    results = {}
    for benchmark in benchmark_list:
        results[benchmark] = run_benchmark_both(A, B, benchmark)
        res = results[benchmark]
        a = res['meanA']
        b = res['meanB']
        diff = a-b
        percDiff = diff / max(a,b)
        print(benchmark, "\t%.3fs" % a, "\t%.3fs" % b, "\t( %.3f%%" % (percDiff * 100.0), "\t, %.3f sigma)" % abs(res['res']))
    return results

# read the initial benchmark list
with open(benchmark_file, 'r') as f:
    benchmark_list = [x.strip() for x in f.read().split('\n') if len(x) > 0]

multiply_percentage = 1.0 + regression_threshold_percentage
outliers = []
for i in range(number_repetitions):
    a_list = []
    unknown_list = []
    b_list = []
    if len(benchmark_list) == 0:
        break

    result = run_benchmarks(old_runner, new_runner, benchmark_list)

exit_code = 0

print()

exit(exit_code)
