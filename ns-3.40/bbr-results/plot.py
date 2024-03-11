import matplotlib.pyplot as plt
import numpy as np
def parse_line(line):
    # Assuming the format of each line is: time src -> dst throughput
    parts = line.split()
    time = float(parts[0]) / 1e9
    #print (time)
    src = parts[1]
    dst = parts[3]
    throughput = float(parts[4])
    return time, src, dst, throughput

def read_data_from_file(file_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()
        # Skip the header if present
        lines = lines[2:]
        data = [parse_line(line) for line in lines]
    return data

# Replace 'your_file.txt' with the actual path to your file
file_path = 'throughput.dat'
data = read_data_from_file(file_path)

# Filter data for sources 10.2.1.1 and 10.2.2.1
filtered_data = [(src, time, throughput) for time, src, dst, throughput in data if src in ['10.2.1.1', '10.2.2.1']]

# Separate data for each source
source_1_data = [(time, throughput) for src, time, throughput in filtered_data if '10.2.1.1' in src]
source_2_data = [(time, throughput) for src, time, throughput in filtered_data if '10.2.2.1' in src]

# Plotting
plt.subplot(2, 1, 1)  # 2 rows, 1 column, first subplot
plt.plot(*zip(*source_1_data), label='10.2.1.1')
plt.plot(*zip(*source_2_data), label='10.2.2.1')

# Adding labels and title
plt.xlabel('Time (s)')
plt.ylabel('Throughput')
plt.title('Throughput over Time for RTT X<ratio>')
plt.legend()

#plotting the throughput ratio graph
plt.subplot(2, 1, 2)  # 2 rows, 1 column, second subplot
throughput1 = np.array([throughput for time,throughput in source_1_data])
throughput2 = np.array([throughput for time,throughput in source_2_data])
time_arr = [time for time,throughput in source_1_data]

plt.plot(time_arr, throughput1/throughput2)
plt.xlabel('Time (s)')
plt.ylabel('Throughput1/Throughput2')

plt.tight_layout()
# Display the plot
plt.show()

