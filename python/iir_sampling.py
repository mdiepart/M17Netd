import numpy as np
from scipy import signal
import matplotlib.pyplot as plt


if __name__ == "__main__":
    sfNum = [4.24433681e-05, 8.48867363e-05, 4.24433681e-05]
    sfDen = [1.0, -1.98148851, 0.98165828]

    w, h = signal.freqz(sfNum, sfDen, fs=24000, worN=4096)
    plt.plot(w, 20*np.log10(abs(h)))
    plt.plot(w, np.rad2deg(np.angle(h)))


    b, a = signal.iirfilter(2, Wn=50, btype='lowpass', output='ba', fs=96000)
    print(b)
    print(a)
    w2, h2 = signal.freqz(b, a, fs=96000, worN=4096)
    plt.plot(w2, 20*np.log10(abs(h2)))
    plt.plot(w2, np.rad2deg(np.angle(h2)))

    plt.show()