import os
import glob
import numpy as np
import matplotlib.pyplot as plt

from matplotlib.ticker import FuncFormatter

# ============================================================
# SETTINGS
# ============================================================

# Plot types:
# "all_responses"
# "mean_responses"
# "spatial_std"
# "avg_spatial_std"

PLOT_TYPE = "mean_responses"

# Use calibrated or non-calibrated measurements
USE_CAL = True

# Frequency limits
F_MIN = 25
F_MAX = 20000

# Y-axis limits
Y_MIN = -3.0
Y_MAX = 3.0

# Frequency range used for normalization
NORM_MIN = 500
NORM_MAX = 2000

# Response smoothing
ENABLE_SMOOTHING = True
SMOOTH_WINDOW = 5

# Trendline smoothing
TREND_WINDOW = 15

# Root directory
ROOT_DIR = os.path.dirname(os.path.abspath(__file__))

# Measurement folders
MEASUREMENT_FOLDERS = {
    #1:  "1 Mic",
    3:  "3 Mic",
    5:  "5 Mic",
    10: "10 Mic",
    12: "12 Mic",
}

# Curves shown in the "mean_responses" plot
MEAN_RESPONSE_PLOT_POINTS = [3, 5]

# ============================================================
# CUSTOM COLORS
# ============================================================

COLORS = [
    "#0B1F3A",  # deep navy
    "#174A7E",  # dark blue
    "#2E6FAD",  # medium blue
    "#5FA8D3",  # light blue
    "#A9D6E5",  # very light blue
]

MEAN_COLOR = "#D62828"

# ============================================================
# HELPER FUNCTIONS
# ============================================================

def smooth(data, window):

    if window <= 1:
        return data

    kernel = np.ones(window) / window

    return np.convolve(data, kernel, mode='same')

# ============================================================

def format_frequency(x, pos):

    if x >= 1000:
        return f"{int(x/1000)} kHz"

    return f"{int(x)} Hz"

# ============================================================
# LOAD DATA
# ============================================================

data = {}

for n_points, folder in MEASUREMENT_FOLDERS.items():
