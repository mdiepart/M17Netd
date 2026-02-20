import numpy as np
import scipy
from scipy import *
import matplotlib.pyplot as plt


def gen_kaiser_lpf(cutoff_freq, samp_freq, transition_width, atten, gain):
    nft = transition_width/samp_freq
    N, beta = scipy.signal.kaiserord(atten, nft)
    taps = scipy.signal.firwin(N, cutoff_freq, window=('kaiser', beta), fs=samp_freq, scale=gain)
    return taps

def gen_least_square_lpf(N, cutoff_freq, samp_freq, transition_width, atten, gain):

    bands = [0, cutoff_freq-0.5*transition_width, cutoff_freq+0.5*transition_width, samp_freq/2]
    taps = scipy.signal.firls(N, bands, [gain, gain, atten, atten], fs=samp_freq)
    #taps = scipy.signal.firwin(N, cutoff_freq, window='hamming', fs=samp_freq, scale=gain)
    return taps

def gen_freqs_vector(fs, len):
    return np.linspace(0, fs/2, len)

def plot_comparison(taps1, taps2, fs):
    fft_taps1 = scipy.fft.rfft(taps1)
    fft_taps2 = scipy.fft.rfft(taps2)
    len1 = len(fft_taps1)
    len2 = len(fft_taps2)
    N = min(len1, len2)

    fig, ax = plt.subplots()
    f = gen_freqs_vector(fs, N)
    g1 = 10*np.log10(np.abs(fft_taps1[0:N]))
    g2 = 10*np.log10(np.abs(fft_taps2[0:N]))
    ax.plot(f, g1)
    ax.plot(f, g2)
    ax.legend(["Taps1", "Taps2"])



def plot_freq_resp(taps, fs):
    fft_taps = scipy.fft.rfft(taps)

    y1 = 10*np.log10(abs(fft_taps))
    y2 = np.rad2deg(np.angle(fft_taps))

    f = gen_freqs_vector(fs, len(fft_taps))

    fig, ax1 = plt.subplots()
    ax2 = ax1.twinx()

    ax1.plot(f, y1, 'b-')
    ax2.plot(f, y2, 'g-')
    ax1.set_xlabel("Frequency")
    ax1.set_ylabel("Gain [dB]")
    ax2.set_ylabel("Phase (deg)")
    plt.title("Frequency response")

def print_taps(taps):
    print("taps = {")
    for i in range(0, len(taps)):
        print(f"{taps[i]},", end="")
        if( (i%4) == 0 ):
            print("")
    print("}")


if __name__ == "__main__":
    fs = 96000
    taps = gen_kaiser_lpf(4900, fs, 3000, 30, 1)
    taps = np.float32(taps)

    if( (len(taps)%2) == 0):
        N = len(taps)+1
    else:
        N = len(taps)
    print(["Using", N, "taps"])

    taps2 = gen_least_square_lpf(N, 4900, fs, 3000, 0, 1)
    taps2 = np.float32(taps2)

    plot_freq_resp(taps, fs)
    plot_freq_resp(taps2, fs)
    plot_comparison(taps, taps2, fs)

    print_taps(taps2)
    plt.show()