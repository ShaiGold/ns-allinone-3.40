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

# Plotting# Create two subplots with shared x-axis
fig, (ax1, ax2) = plt.subplots(2, 1, sharex=True)

# Plotting throughput data on the first subplot
ax1.plot(*zip(*source_1_data), label='10.2.1.1')
ax1.plot(*zip(*source_2_data), label='10.2.2.1')
ax1.set_ylabel('Throughput')
ax1.set_title('Throughput over Time for RTT X<ratio>')
ax1.legend()

# Plotting the throughput ratio graph on the second subplot
throughput1 = np.array([throughput for time, throughput in source_1_data])
throughput2 = np.array([throughput for time, throughput in source_2_data])
time_arr = [time for time, throughput in source_1_data]

ax2.plot(time_arr, throughput1 / throughput2, label='Throughput Ratio')
ax2.axhline(y=1, color='red', linestyle='--', label='Throughput Ratio = 1')
ax2.set_xlabel('Time (s)')
ax2.set_ylabel('Throughput1/Throughput2')
ax2.legend()

# Adjust layout to prevent overlapping
plt.tight_layout()

# Display the plot
plt.show()
