import commpy
import numpy as np
import math
import scipy
import csv
from matplotlib import pyplot as plt

def do_taps():
    (_, taps) = commpy.filters.rrcosfilter(161, 0.5, 1/4800, 96000)

    #taps = taps/sum(taps)
    taps = taps/sum(taps)
    taps = taps*math.sqrt(20)

    return taps

def print_taps(taps):
    j = 1
    for i in taps:
        print("{: 2.18f}f, ".format(i), end='')
        if (j%4) == 0:
            print("\n", end='')
        j = j+1

def filter_symbols(symbols, taps):
    interpolated_sig = []
    half_len = round(len(taps)/2)

    for s in symbols:
        interpolated_sig.append(s)
        for i in range(19):
            interpolated_sig.append(0)
    
    for i in range(half_len):
        interpolated_sig.append(0)
    

    print("Interpolated len: ", end='')
    print(len(interpolated_sig))
    filtered = scipy.signal.lfilter(taps, [1.0], interpolated_sig)

    #filtered = filtered[(half_len)-1:-1]
    #interpolated_sig = interpolated_sig[0:-half_len]
    print("Filtered len:", end='')
    print(len(filtered))
    t = np.linspace(0, len(filtered)/96000, num=len(filtered))

    return(t, interpolated_sig, filtered)

def read_sym_csv(filename):
    with open(filename, newline='') as csvfile:
        t = []
        syms = []

        csvreader = csv.reader(csvfile, delimiter=',')
        j=0
        for row in csvreader:
            if(j > 0):
                t.append(float(row[0]))
                syms.append(float(row[1]))
            j = j+1

        return(t, syms)

def read_bb_csv(filename):
    with open(filename, newline='') as csvfile:
        t = []
        bb = []

        csvreader = csv.reader(csvfile, delimiter=',')
        j=0
        for row in csvreader:
            if(j>0):
                t.append(float(row[0]))
                bb.append(float(row[1]))
            j = j+1

        return(t, bb)

def compute_freq_resp(taps):
    [freqs, resp] = scipy.signal.freqz(taps, worN=2048, fs=96000)
    return (freqs, resp)

if __name__ == "__main__":
    taps = do_taps()
    print("Taps len:", end='')
    print(len(taps))

    #print_taps(taps)

    #(t_sym, syms) = read_sym_csv("./build/0_sym.csv")
    #(t_bb, bb) = read_bb_csv("./build/0.csv")

    #plt.plot(t_sym, syms)
    #(t, interpolated_sig, filtered) = filter_symbols(syms, taps)
    #plt.plot(t_bb, bb)
    #plt.plot(t, filtered)

    (freqs, resp) = compute_freq_resp(taps)
    plt.plot(freqs, 10*np.log10(abs(resp)))
    plt.show()

    
    #(t, interpolated_sig, filtered) = filter_symbols(baseband, taps)
    #plt.plot(t, filtered)
    #plt.plot(t, interpolated_sig)
    #plt.show()