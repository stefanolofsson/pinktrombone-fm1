#!/usr/bin/env python3
"""Estimate formants of the voice with LPC. python3 host/formants.py file.raw"""
import sys
import numpy as np


def formants(x, sr=44100, order=12):
    k = 4                                   # decimate to 11 kHz: the formants are below 5 kHz
    x = x[: len(x) // k * k].reshape(-1, k).mean(axis=1)
    sr //= k
    x = np.append(x[0], x[1:] - 0.97 * x[:-1]) * np.hanning(len(x))
    r = np.correlate(x, x, 'full')[len(x) - 1:len(x) + order]
    T = np.array([[r[abs(i - j)] for j in range(order)] for i in range(order)])
    a = np.linalg.solve(T + 1e-9 * r[0] * np.eye(order), -r[1:order + 1])
    roots = [z for z in np.roots(np.concatenate([[1], a])) if np.imag(z) > 0.01]
    fr = sorted((np.angle(z) * sr / (2 * np.pi), -sr / np.pi * np.log(abs(z))) for z in roots)
    return [int(round(f)) for f, bw in fr if f > 150 and bw < 700]


if __name__ == '__main__':
    x = np.fromfile(sys.argv[1], dtype=np.float32)
    seg = x[len(x) // 2: len(x) // 2 + 4096]
    print('peak %.3f rms %.3f' % (np.abs(x).max(), np.sqrt((x ** 2).mean())), 'formants', formants(seg)[:5])
