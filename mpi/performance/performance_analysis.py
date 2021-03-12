import os
import csv
import sys
import platform
import itertools
import subprocess as sp
import multiprocessing as mp
from statistics import mean

if __name__ == "__main__":
    cpu_count = mp.cpu_count()
    pc_name = platform.node()

    print("Cores:", cpu_count)
    print("PC:", pc_name)

    grid_sizes = [(1024, 1024), (2048, 2048), (4096, 4096)]
    # get possible x/y process combinations
    x_processes = [x+1 for x in range(cpu_count)]
    y_processes = x_processes
    processes = [x for x in itertools.product(x_processes, y_processes) if x[0]*x[1] <= cpu_count and x[0]*x[1] != 3]
    processes.sort(key=lambda t: t[0]*t[1])

    # get arguments
    time_steps = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 100
    iterations = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[1].isdigit() else 5

    # list of tuples (PC, #Cores, grid-X, grid-Y, #processes, #processes-X, #processes-Y, process-Width, process-Height, times)
    # times = list of elapsed seconds 
    header_row = ("PC", "#Cores", "grid-X", "grid-Y", "#processes", "#processes-X", "#processes-Y", "process-Width", "process-Height", "times", "AVG(time)", "memory", "AVG(memory)")
    result_list = []

    for total_width, total_height in grid_sizes:
        print("Grid Size:", total_width, "x", total_height)
        
        for process_count_x, process_count_y in processes:
            print("processes:", process_count_x * process_count_y, "X:", process_count_x, "Y:", process_count_y)
            process_width = total_width // process_count_x
            process_height = total_height // process_count_y

            times = []
            mem = []
            for i in range(iterations):
                result = sp.run(["/usr/bin/time", "--format=SEC:%e,MEM:%M", "mpirun", "-n", str(process_count_x * process_count_y), "../gameoflife", str(time_steps), str(process_width), str(process_height), str(process_count_x), str(process_count_y)], stderr=sp.PIPE)
                time_output = result.stderr.decode("utf-8").split(",")
                elapsed_s = float(time_output[0][4:])
                memory_kb = int(time_output[1][4:])
                print("ITERATION", i, "Elapsed time:", elapsed_s)
                times.append(elapsed_s)
                mem.append(memory_kb)
            result_tuple = (pc_name, cpu_count, total_width, total_height, process_count_x * process_count_y, process_count_x, process_count_y, process_width, process_height, times, mean(times), mem, mean(mem))
            result_list.append(result_tuple)
    
with open("results_"+pc_name+".csv", "w") as out:
    writer = csv.writer(out)
    writer.writerow(header_row)
    for row in result_list:
        writer.writerow(row)
