import pandas as pd
import matplotlib.pyplot  as plt
import json
#import numpy as np
from scipy.signal import savgol_filter

# filename = 'espsonda_08-50-03_04-03-2025'
filename = 'espsonda_09-22-08_12-12-2025'


with open(f'measurements\\{filename}.json') as json_file:
    json_decoded = json.load(json_file)

# with open('espsonda_09-42-16_29-01-2025.json') as json_file_2:
#     json_decoded_2 = json.load(json_file_2)

# Assuming json_decoded["board"] contains all channel data
# channels = [f"CH{i}" for i in range(16)]  # List of channels CH0 to CH15
channels = [f"CH{i}" for i in range(16)]  # List of channels CH0 to CH15

# Extracting voltage values for each channel
channel_values = {channel: [entry[0]["voltages"].get(channel, None) for entry in json_decoded["board"]] for channel in channels}

for key, value in {**json_decoded, **channel_values}.items():
    print(key, len(value))


all_arrays = {
    "epochtime": json_decoded["epochtime"],
    "current": json_decoded["current"],
    **channel_values,   # CH0..CH15
    "board": json_decoded["board"],
}

min_len = min(len(v) for v in all_arrays.values())

for key in all_arrays:
    all_arrays[key] = all_arrays[key][:min_len]

df = pd.DataFrame(all_arrays)


# Creating the DataFrame
# df = pd.DataFrame({
#     "epochtime": json_decoded["epochtime"],
#     **channel_values,
#     "current": json_decoded["current"]
# })

# Convert epochtime to datetime
df['epochtime'] =( pd.to_datetime(df['epochtime'], unit='s')
                  .dt.tz_localize("UTC")  
                  .dt.tz_convert("Europe/Warsaw"))

#df = df.truncate(after = 2400)
#df = df.truncate(before = 1000)

# Print the DataFrame
print(df)

channels_per_fig = 4
num_figs = len(channels) // channels_per_fig

for fig_index in range(num_figs):
    # plt.figure(figsize=(12, 8))  # size per figure

    # subset of 4 channels for this figure
    subset = channels[fig_index * channels_per_fig : (fig_index + 1) * channels_per_fig]


# Plotting all channels
    fig = plt.figure(figsize=(10, 10))  # Adjust the figure size for better visibility


    for i, channel in enumerate(subset):
        ax1 = plt.subplot(2, 2, i + 1)  # 4x4 grid for 16 subplots
        ax1.plot(df["epochtime"], df[channel], label=f"{channel} vs Time")
        ax1.set_ylabel("Voltage")

        ax2 = ax1.twinx()
        ax2.plot(df["epochtime"], df["current"], color='r', label=f"Current vs Time")
        ax2.set_ylabel("Current")

        ax1.set_title(channel)
        ax1.set_xlabel("Time")
        ax1.tick_params(axis='x', rotation=90)

        lines, labels = ax1.get_legend_handles_labels()
        lines2, labels2 = ax2.get_legend_handles_labels()
        # ax1.legend(lines + lines2, labels + labels2, loc='upper left')
        ax1.legend(lines + lines2, labels + labels2, loc='lower right')

        #plt.legend()
        
        ax1.grid()
        plt.tight_layout()
    plt.savefig(f'graphs/{filename}_fig{fig_index+1}.png')
    plt.show()


# Apply Savitzky-Golay filter to smooth the data
smoothed_data = {}
for channel in channels:
    signal = df[channel]#.dropna()  # Remove NaN values
    smoothed_signal = savgol_filter(signal, 101, 3)  # Window size 101, polynomial order 3
    smoothed_data[channel] = smoothed_signal

# Add smoothed data to DataFrame
for channel, smoothed_signal in smoothed_data.items():
    df[f"{channel}_smoothed"] = smoothed_signal

# Plot the original and smoothed data for each channel
plt.figure(figsize=(20, 20))  # Adjust the figure size for better visibility
for i, channel in enumerate(channels):
    plt.subplot(4, 4, i + 1)  # 4x4 grid for 16 subplots
    plt.plot(df["epochtime"], df[channel], linestyle='-', linewidth=2, alpha=0.5, label=f"{channel} (Original)")
    plt.plot(df["epochtime"], df[f"{channel}_smoothed"], color='r', label=f"{channel} (Smoothed)")
    plt.title(channel)
    plt.xlabel("Time")
    plt.xticks(rotation=45)
    plt.ylabel("Voltage")
    plt.legend(loc=2)
    plt.grid()
    plt.tight_layout()
plt.savefig(f'graphs\\{filename}_2.png')
plt.show()


# Sampling frequency (Hz) - adjust this based on your data
# fs = 10  # Example: 10 samples per second

# # Plotting the FFT of all channels
# plt.figure(figsize=(20, 20))  # Adjust the figure size for better visibility

# for i, channel in enumerate(channels):
#     # Remove NaN values from the channel for FFT
#     signal = df[channel].dropna()

#     # Perform FFT
#     n = len(signal)
#     freqs = np.fft.rfftfreq(n, d=1/fs)  # Frequency bins
#     fft_values = np.fft.rfft(signal)    # FFT values
#     magnitude = np.abs(fft_values)      # Magnitude of FFT
    
#     # Plot FFT
#     plt.subplot(4, 4, i + 1)  # 4x4 grid for 16 subplots
#     plt.plot(freqs, magnitude, label=f"FFT of {channel}")
#     plt.title(f"FFT of {channel}")
#     plt.xlabel("Frequency (Hz)")
#     plt.ylabel("Magnitude")
#     plt.legend()
#     plt.tight_layout()

# plt.show()

