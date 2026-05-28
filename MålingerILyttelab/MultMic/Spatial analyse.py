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

PLOT_TYPE = "spatial_std"

# Use calibrated or non-calibrated measurements
USE_CAL = True

# Frequency limits
F_MIN = 25
F_MAX = 20000

# Y-axis limits
Y_MIN = 0.0
Y_MAX = 4.0

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

# Curves shown in the "spatial_std" plot
SPATIAL_STD_PLOT_POINTS = [3, 5]

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

    folder_path = os.path.join(ROOT_DIR, folder)

    print()
    print(f"Loading {n_points} point dataset")

    # ========================================================
    # FIND FILES
    # ========================================================

    all_files = glob.glob(os.path.join(folder_path, "*.txt"))
    all_files += glob.glob(os.path.join(folder_path, "*.TXT"))

    files = []

    for file in all_files:

        filename = os.path.basename(file).lower()

        if USE_CAL:

            # Include CAL
            # Exclude NO CAL
            if "cal" in filename and "no cal" not in filename:
                files.append(file)

        else:

            # Include only NO CAL
            if "no cal" in filename:
                files.append(file)

    # ========================================================
    # LOAD RESPONSES
    # ========================================================

    responses = []

    for filepath in sorted(files):

        print(filepath)

        raw = np.loadtxt(filepath)

        freq = raw[:, 0]
        mag  = raw[:, 1]

        # ====================================================
        # NORMALIZATION
        # ====================================================

        mask = (freq >= NORM_MIN) & (freq <= NORM_MAX)

        reference_level = np.mean(mag[mask])

        mag = mag - reference_level

        # ====================================================
        # SMOOTHING
        # ====================================================

        if ENABLE_SMOOTHING:
            mag = smooth(mag, SMOOTH_WINDOW)

        responses.append(mag)

    responses = np.array(responses)

    data[n_points] = {
        "freq": freq,
        "responses": responses
    }

# ============================================================
# SORT KEYS
# ============================================================

sorted_keys = sorted(data.keys())

# ============================================================
# PLOT 1
# ALL RESPONSES
# ============================================================

if PLOT_TYPE == "all_responses":

    for idx, n_points in enumerate(sorted_keys):

        plt.figure(figsize=(10, 6))

        freq = data[n_points]["freq"]
        responses = data[n_points]["responses"]

        # ====================================================
        # PLOT ALL RESPONSES
        # ====================================================

        for i, response in enumerate(responses):

            response_color = COLORS[
                i % len(COLORS)
            ]

            plt.semilogx(
                freq,
                response,
                linewidth=1.5,
                alpha=0.35,
                color=response_color
            )

        # ====================================================
        # MEAN RESPONSE
        # ====================================================

        mean_response = np.mean(
            responses,
            axis=0
        )

        plt.semilogx(
            freq,
            mean_response,
            linewidth=2,
            linestyle='--',
            color=MEAN_COLOR,
            label='Mean'
        )

        # ====================================================
        # PLOT SETTINGS
        # ====================================================

        plt.xlim(F_MIN, F_MAX)
        plt.ylim(Y_MIN, Y_MAX)

        plt.xlabel("Frequency [Hz]")
        plt.ylabel("Magnitude [dB]")

        plt.title(
            f"Corrected Responses - {n_points} Measurement Points"
        )

        plt.grid(True, which="both")
        plt.legend()

        plt.gca().xaxis.set_major_formatter(
            FuncFormatter(format_frequency)
        )

        plt.tight_layout()

        plt.savefig(
            f"all_responses_{n_points}.png",
            dpi=600,
            bbox_inches='tight'
        )

    plt.show()

# ============================================================
# PLOT 2
# MEAN RESPONSES
# ============================================================

elif PLOT_TYPE == "mean_responses":

    plt.figure(figsize=(10, 6))

    all_mean_curves = []

    for idx, n_points in enumerate(sorted_keys):

        freq = data[n_points]["freq"]
        responses = data[n_points]["responses"]

        # ====================================================
        # MEAN RESPONSE
        # ====================================================

        mean_response = np.mean(
            responses,
            axis=0
        )

        all_mean_curves.append(mean_response)

        # ====================================================
        # AVERAGE VARIATION FROM 0 dB
        # ====================================================

        avg_variation = np.mean(
            np.abs(mean_response)
        )

        # ====================================================
        # PLOT
        # ====================================================

        plt.semilogx(
            freq,
            mean_response,
            linewidth=2,
            color=COLORS[idx % len(COLORS)],
            label=(
                f"{n_points} points "
                f"(var: {avg_variation:.2f} dB)"
            )
        )

    # ========================================================
    # GLOBAL MEAN TREND
    # ========================================================

    overall_mean = np.mean(
        all_mean_curves,
        axis=0
    )

    trend = smooth(
        overall_mean,
        TREND_WINDOW
    )

    plt.semilogx(
        freq,
        trend,
        linewidth=2.5,
        linestyle='--',
        color=MEAN_COLOR,
        label='Overall mean'
    )

    # ========================================================
    # ±1.5 dB REQUIREMENT LINES
    # ========================================================

    plt.axhline(
        1.5,
        color='black',
        linestyle=':',
        linewidth=2,
        alpha=0.8
    )

    plt.axhline(
        -1.5,
        color='black',
        linestyle=':',
        linewidth=2,
        alpha=0.8
    )

    # ========================================================
    # PLOT SETTINGS
    # ========================================================

    plt.xlim(F_MIN, F_MAX)
    plt.ylim(Y_MIN, Y_MAX)

    plt.xlabel("Frequency [Hz]")
    plt.ylabel("Magnitude [dB]")

    plt.title(
        "Spatially Averaged Corrected Responses"
    )

    plt.grid(True, which="both")
    plt.legend()

    plt.gca().xaxis.set_major_formatter(
        FuncFormatter(format_frequency)
    )

    plt.yticks(np.arange(Y_MIN, Y_MAX + 0.5, 0.5))

    plt.tight_layout()

    output_path = os.path.join(ROOT_DIR, "mean_responses.png")

    plt.savefig(
        output_path,
        dpi=600,
        bbox_inches='tight'
    )

    print(f"Saved plot to: {output_path}")

    plt.show()

# ============================================================
# PLOT 3
# SPATIAL STD VS FREQUENCY
# ============================================================

elif PLOT_TYPE == "spatial_std":

    plt.figure(figsize=(10, 6))

    all_std_curves = []
    spatial_std_keys = [
        n_points for n_points in sorted_keys
        if n_points in SPATIAL_STD_PLOT_POINTS
    ]

    for idx, n_points in enumerate(spatial_std_keys):

        freq = data[n_points]["freq"]
        responses = data[n_points]["responses"]

        spatial_std = np.std(responses, axis=0)

        all_std_curves.append(spatial_std)

        color = COLORS[idx % len(COLORS)]

        # ====================================================
        # RAW STD CURVES
        # ====================================================

        plt.semilogx(
            freq,
            spatial_std,
            linewidth=1.5,
            alpha=0.75,
            color=color,
            label=f"{n_points} points"
        )

    # ========================================================
    # GLOBAL TRENDLINE
    # ========================================================

    mean_std = np.mean(all_std_curves, axis=0)

    trend = smooth(mean_std, TREND_WINDOW)

    plt.semilogx(
        freq,
        trend,
        linewidth=2.5,
        linestyle='--',
        color=MEAN_COLOR,
        label='Overall mean'
    )

    # ========================================================
    # PLOT SETTINGS
    # ========================================================

    plt.xlim(F_MIN, F_MAX)
    plt.ylim(Y_MIN, Y_MAX)

    plt.xlabel("Frequency")
    plt.ylabel("Standard Deviation [dB]")

    plt.title("Spatial Standard Deviation")

    plt.grid(True, which="both")
    plt.legend()

    plt.gca().xaxis.set_major_formatter(
        FuncFormatter(format_frequency)
    )

    plt.yticks(np.arange(Y_MIN, Y_MAX + 0.5, 0.5))

    plt.tight_layout()

    output_path = os.path.join(ROOT_DIR, "spatial_std.png")

    plt.savefig(
        output_path,
        dpi=600,
        bbox_inches='tight'
    )

    print(f"Saved plot to: {output_path}")

    plt.show()

# ============================================================
# PLOT 4
# AVERAGE SPATIAL STD
# ============================================================

elif PLOT_TYPE == "avg_spatial_std":

    avg_std_values = []

    for n_points in sorted_keys:

        responses = data[n_points]["responses"]

        spatial_std = np.std(responses, axis=0)

        avg_std = np.mean(spatial_std)

        avg_std_values.append(avg_std)

    plt.figure(figsize=(8, 5))

    plt.plot(
        sorted_keys,
        avg_std_values,
        marker='o',
        markersize=8,
        linewidth=2.5,
        color=COLORS[0]
    )

    plt.xlim(min(sorted_keys), max(sorted_keys))
    plt.ylim(Y_MIN, Y_MAX)

    plt.xlabel("Number of Measurement Points")
    plt.ylabel("Average Spatial Std [dB]")

    plt.title(
        "Average Spatial Variation After Correction"
    )

    plt.grid(True)

    plt.tight_layout()

    plt.savefig(
        "avg_spatial_std.png",
        dpi=600,
        bbox_inches='tight'
    )

    plt.show()

# ============================================================
# INVALID OPTION
# ============================================================

else:

    print("Invalid PLOT_TYPE selected.")
