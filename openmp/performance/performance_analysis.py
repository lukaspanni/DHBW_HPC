import os
import re
import csv
import sys
import platform
import itertools
import subprocess as sp
import multiprocessing as mp

if __name__ == "__main__":
    cpu_count = mp.cpu_count()
    pc_name = platform.node()

    print("Cores:", cpu_count)
    print("PC:", pc_name)

    grid_sizes = [(1024, 1024), (2048, 2048), (4096, 4096)]
    # get possible x/y thread combinations
    x_threads = [x+1 for x in range(cpu_count)]
    y_threads = x_threads
    threads = [x for x in itertools.product(x_threads, y_threads) if x[0]*x[1] <= cpu_count]
    threads.sort(key=lambda t: t[0]*t[1])

    # get arguments
    time_steps = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 100
    iterations = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[1].isdigit() else 5

    # list of tuples (PC, #Cores, grid-X, grid-Y, #threads, #threads-X, #threads-Y, thread-Width, thread-Height, times)
    # times = list of elapsed seconds 
    header_row = ("PC", "#Cores", "grid-X", "grid-Y", "#threads", "#threads-X", "#threads-Y", "thread-Width", "thread-Height", "times")
    result_list = []

    for total_width, total_height in grid_sizes:
        print("Grid Size:", total_width, "x", total_height)
        
        for thread_count_x, thread_count_y in threads:
            print("Threads:", thread_count_x * thread_count_y, "X:", thread_count_x, "Y:", thread_count_y)
            thread_width = total_width // thread_count_x
            thread_height = total_height // thread_count_y

            times = []
            for i in range(iterations):
                result = sp.run(["time", "../gameoflife", str(time_steps), str(thread_width), str(thread_height), str(thread_count_x), str(thread_count_y)], stderr=sp.PIPE)
                
                elapsed = str(result.stderr.split()[2])
                time_search = re.search("([0-9]+):([0-9]+).([0-9]+)", elapsed)
                if not time_search:
                    raise RuntimeError

                minutes = time_search.group(1)
                seconds = time_search.group(2)
                milis = time_search.group(3)

                print("ITERATION", i, "Elapsed time:", minutes,"minutes", seconds, "seconds", milis, "miliseconds")
                seconds_total = int(minutes) * 60 + int(seconds) + int(milis)/1000
                times.append(seconds_total)
            result_tuple = (pc_name, cpu_count, total_width, total_height, thread_count_x * thread_count_y, thread_count_x, thread_count_y, thread_width, thread_height, times)
            result_list.append(result_tuple)
    
with open("results_"+pc_name+".csv", "w") as out:
    writer = csv.writer(out)
    writer.writerow(header_row)
    for row in result_list:
        writer.writerow(row)
