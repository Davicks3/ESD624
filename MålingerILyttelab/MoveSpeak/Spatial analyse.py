import os
import glob
import numpy as np
import matplotlib.pyplot as plt

from matplotlib.ticker import FuncFormatter

# ============================================================
# SETTINGS
# ============================================================

# Plot types:
# "responses"
# "mean_comparison"
# "difference"

PLOT_TYPE = "mean_comparison"

# Use calibrated or non-calibrated measurements
USE_CAL = True

# Frequency limits
F_MIN = 25
F_MAX = 20000

# Y-axis limits
Y_MIN = -6
Y_MAX = 3

# Frequency range used for normalization
NORM_MIN = 500
NORM_MAX = 2000

# Response smoothing
ENABLE_SMOOTHING = True
SMOOTH_WINDOW = 5

# Root directory
ROOT_DIR = os.path.dirname(os.path.abspath(__file__))

# ============================================================
# MEASUREMENT FOLDERS
# ============================================================

MEASUREMENT_FOLDERS = {
    "BackWall": "BackWall",
    "Cornor": "Cornor",
}

# ============================================================
# CUSTOM COLORS
# ============================================================

COLORS = {
    "BackWall": "#174A7E",
    "Cornor": "#D62828",
}

# ============================================================
# HELPER FUNCTIONS
# ============================================================

def smooth(data, window):

    if window <= 1:
        return data

    kernel = np.ones(window) / window

    return np.convolve(
        data,
        kernel,
        mode='same'
    )

# ============================================================

def format_frequency(x, pos):

    if x >= 1000:
        return f"{int(x/1000)} kHz"

    return f"{int(x)} Hz"

# ============================================================
# LOAD DATA
# ============================================================

data = {}

for placement, folder in MEASUREMENT_FOLDERS.items():

    folder_path = os.path.join(
        ROOT_DIR,
        folder
    )

    print()
    print(f"Loading {placement}")

    # ========================================================
    # FIND FILES
    # ========================================================

    all_files = glob.glob(
        os.path.join(folder_path, "*.txt")
    )

    files = []

    for file in all_files:

        filename = os.path.basename(file).lower()

        if USE_CAL:

            if (
                "cal" in filename and
                "no cal" not in filename
            ):
                files.append(file)

        else:

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

        mask = (
            (freq >= NORM_MIN) &
            (freq <= NORM_MAX)
        )

        reference_level = np.mean(
            mag[mask]
        )

        mag = mag - reference_level

        # ====================================================
        # SMOOTHING
        # ====================================================

        if ENABLE_SMOOTHING:

            mag = smooth(
                mag,
                SMOOTH_WINDOW
            )

        responses.append(mag)

    # ========================================================
    # CHECK FOR EMPTY DATASET
    # ========================================================

    if len(responses) == 0:

        print(
            f"No valid files found in: {folder_path}"
        )

        continue

    responses = np.array(responses)

    data[placement] = {

        "freq": freq,

        "responses": responses,

        "mean": np.mean(
            responses,
            axis=0
        )
    }

# ============================================================
# PLOT 1
# ALL RESPONSES
# ============================================================

if PLOT_TYPE == "responses":

    for placement in data.keys():

        plt.figure(figsize=(10, 6))

        freq = data[placement]["freq"]

        responses = data[placement]["responses"]

        # ====================================================
        # PLOT INDIVIDUAL RESPONSES
        # ====================================================

        for response in responses:

            plt.semilogx(
                freq,
                response,
                linewidth=1.5,
                alpha=0.35,
                color=COLORS[placement]
            )

        # ====================================================
        # PLOT MEAN
        # ====================================================

        mean_response = data[placement]["mean"]

        plt.semilogx(
            freq,
            mean_response,
            linewidth=2.5,
            linestyle='--',
            color='black',
            label='Mean'
        )

        # ====================================================
        # REQUIREMENT LINES
        # ====================================================

        plt.axhline(
            1.5,
            color='gray',
            linestyle=':',
            linewidth=1.5,
            alpha=0.8
        )

        plt.axhline(
            -1.5,
            color='gray',
            linestyle=':',
            linewidth=1.5,
            alpha=0.8
        )

        # ====================================================
        # SETTINGS
        # ====================================================

        plt.xlim(F_MIN, F_MAX)
        plt.ylim(Y_MIN, Y_MAX)

        plt.xlabel("Frequency [Hz]")
        plt.ylabel("Magnitude [dB]")

        plt.title(
            f"{placement} Placement"
        )

        plt.grid(True, which="both")
        plt.legend()

        plt.gca().xaxis.set_major_formatter(
            FuncFormatter(format_frequency)
        )

        plt.yticks(
            np.arange(Y_MIN, Y_MAX + 0.5, 0.5)
        )

        plt.tight_layout()

        plt.savefig(
            f"{placement}_responses.png",
            dpi=600,
            bbox_inches='tight'
        )

    plt.show()

# ============================================================
# PLOT 2
# MEAN COMPARISON
# ============================================================

elif PLOT_TYPE == "mean_comparison":

    plt.figure(figsize=(10, 6))

    for placement in data.keys():

        freq = data[placement]["freq"]

        mean_response = data[placement]["mean"]

        variation = np.mean(
            np.abs(mean_response)
        )

        plt.semilogx(
            freq,
            mean_response,
            linewidth=1.5,
            color=COLORS[placement],
            label=(
                f"{placement} "
                f"(var: {variation:.2f} dB)"
            )
        )

    # ========================================================
    # REQUIREMENT LINES
    # ========================================================

    #plt.axhline(
    #    1.5,
    #    color='gray',
    #    linestyle=':',
    #    linewidth=1.5,
    #    alpha=0.8
    #)

    #plt.axhline(
    #    -1.5,
    #    color='gray',
    #    linestyle=':',
    #    linewidth=1.5,
    #    alpha=0.8
    #)

    # ========================================================
    # SETTINGS
    # ========================================================

    plt.xlim(F_MIN, F_MAX)
    plt.ylim(Y_MIN, Y_MAX)

    plt.xlabel("Frequency")
    plt.ylabel("Magnitude [dB]")

    plt.title(
        "Average Corrected Response"
    )

    plt.grid(True, which="both")
    plt.legend()

    plt.gca().xaxis.set_major_formatter(
        FuncFormatter(format_frequency)
    )

    plt.yticks(
        np.arange(Y_MIN, Y_MAX + 0.5, 0.5)
    )

    plt.tight_layout()

    plt.savefig(
        "mean_comparison.png",
        dpi=600,
        bbox_inches='tight'
    )

    plt.show()

# ============================================================
# PLOT 3
# DIFFERENCE PLOT
# ============================================================

elif PLOT_TYPE == "difference":

    plt.figure(figsize=(10, 6))

    freq = data["BackWall"]["freq"]

    difference = (

        data["Cornor"]["mean"]

        -

        data["BackWall"]["mean"]
    )

    plt.semilogx(
        freq,
        difference,
        linewidth=2.5,
        color="#D62828"
    )

    plt.axhline(
        0,
        color='black',
        linestyle='--',
        linewidth=1
    )

    plt.xlim(F_MIN, F_MAX)
    plt.ylim(Y_MIN, Y_MAX)

    plt.xlabel("Frequency [Hz]")
    plt.ylabel("Difference [dB]")

    plt.title(
        "Difference Between Corner and BackWall Placement"
    )

    plt.grid(True, which="both")

    plt.gca().xaxis.set_major_formatter(
        FuncFormatter(format_frequency)
    )

    plt.yticks(
        np.arange(Y_MIN, Y_MAX + 0.5, 0.5)
    )

    plt.tight_layout()

    plt.savefig(
        "placement_difference.png",
        dpi=600,
        bbox_inches='tight'
    )

    plt.show()

# ============================================================
# INVALID OPTION
# ============================================================

else:

    print("Invalid PLOT_TYPE selected.")
