import numpy as np
import matplotlib.pyplot as plt
import sys

# Plot the content of a raw file containing real samples as 32 bits floats
def plot_raw_file(filename, samples):
    arr = np.fromfile(filename, dtype=np.float32)
    plt.plot(arr[samples])
    plt.title(filename)

# Plot the content of a raw file containing I/Q samples as pairs of 32 bits floats
def plot_iq_file(filename, samples):
    iq_arr = np.fromfile(filename, dtype=np.csingle)
    plt.plot(np.real(iq_arr[samples]))
    plt.plot(np.imag(iq_arr[samples]))
    plt.legend(["I", "Q"])
    plt.title(filename)

def plot_pts_file(filename, samples):
    arr = np.fromfile(filename, dtype=np.float32)
    x = arr[::2]-1
    y = arr[1::2]

    lo = min(samples)
    hi = max(samples)

    samps = np.logical_and((x <= hi), (x >= lo))

    plt.plot(x[samps], y[samps], 'o')
    #plt.title(filename)


if __name__ == "__main__":
    argc = len(sys.argv)

    if argc < 3:
        print("Invalid usage")

    nb_samples = int(sys.argv[1])

    samps = range(0, nb_samples)

    for c in range(2, len(sys.argv)):
        filename = sys.argv[c]
        if filename.endswith(".iq"):
            plt.figure()
            plot_iq_file(filename, samps)
        elif filename.endswith(".pts"):
            plot_pts_file(filename, samps)
        else:
            plt.figure()
            plot_raw_file(filename, samps)

    plt.show()