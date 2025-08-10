#!/usr/bin/env python

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import scipy.optimize

data = pd.read_csv("colours.txt", delimiter="\t")

x = np.arange(512)


def r_func(x):
    r = x // 64
    g = (x // 8) % 8
    b = x % 8
    return (
            r * 0.74
            + np.maximum(1, (r / 8 + 0.34)**4)
            * (g / 8)**(1 + np.minimum(r, 3) / 3)
            * 4.4 * (1 -  (r / 8)**1.1)
            + b * 0.01
    ) * 32

def r_fit(x, *p):
    r = x // 64
    g = (x // 8) % 8
    b = x % 8
    return (
            r * p[0]
            + np.maximum(p[1], (r / 8 + p[2])**p[3])
            * (g / p[4])**(p[5] + np.minimum(r, p[6]) / p[7])
            * p[8] * (p[9] -  (r / 8)**p[10])
            + b * p[11]
    ) * p[12]

def r2_fit(x, *p):
    r = x // 64
    g = (x // 8) % 8
    b = x % 8
    return (
            p[0] * r**p[1]
            + p[13] * g**p[14]
            + p[15] * g**p[16]
            + p[2] * (r**p[3] + p[4]) * g**p[5]
            + p[6] * (r**p[7] + p[8]) * (g**p[9] + p[10]) * (b**p[11] + p[12])
            )


popt, pcov = scipy.optimize.curve_fit(r_fit, x, data["R"], p0=[0.74, 1, 0.34, 4, 8,
    1, 3, 3, 4.4, 1, 1.1, 0.01, 32], bounds=(np.zeros(13), 100*np.ones(13)))

popt2, pcov2 = scipy.optimize.curve_fit(r2_fit, x, data["R"],
        p0=np.ones(17),
        bounds=(-100*np.array([1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1,
            0]), 100*np.ones(17)))


print(popt)
print(popt2)

print(r2_fit(8, *popt2))

plt.plot(x, data["R"], label="Data")
plt.plot(x, r_func(x), label="Manual")
plt.plot(x, r_fit(x, *popt), label="Fit")
plt.plot(x, r2_fit(x, *popt2), label="Fit2")
plt.legend()
plt.show()
