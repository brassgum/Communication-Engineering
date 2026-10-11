import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt


def read_fft_csv(csv_path):
    """Read a 2-column FFT CSV file and return frequency and magnitude arrays."""
    rows = []
    with open(csv_path, newline="") as f:
        sample = f.read(4096)
        f.seek(0)
        dialect = csv.Sniffer().sniff(sample, delimiters=",;\t ") if sample else csv.excel
        reader = csv.reader(f, dialect=dialect)

        data = list(reader)

    if not data:
        raise ValueError(f"CSV file is empty: {csv_path}")

    # Try to detect header row.
    first = data[0]
    header_lower = [str(x).strip().lower() for x in first]
    has_header = len(first) >= 2 and any(name in header_lower for name in ["frequency", "freq", "f", "magnitude", "mag", "amplitude"])

    if has_header:
        columns = header_lower
        freq_idx = next((i for i, name in enumerate(columns) if name in {"frequency", "freq", "f"}), None)
        mag_idx = next((i for i, name in enumerate(columns) if name in {"magnitude", "mag", "amplitude", "abs"}), None)

        if freq_idx is None or mag_idx is None:
            raise ValueError("CSV header must contain frequency and magnitude columns.")

        rows = [row for row in data[1:] if len(row) > max(freq_idx, mag_idx)]
        freqs = [float(row[freq_idx]) for row in rows]
        mags = [float(row[mag_idx]) for row in rows]
    else:
        if len(data[0]) < 2:
            raise ValueError("CSV must have at least two columns: frequency and magnitude.")
        freqs = []
        mags = []
        for row in data:
            if len(row) < 2:
                continue
            freqs.append(float(row[0]))
            mags.append(float(row[1]))

    return freqs, mags


def plot_fft(csv_path, output_path=None):
    freqs, mags = read_fft_csv(csv_path)

    plt.figure(figsize=(10, 6))
    plt.plot(freqs, mags, linewidth=2, color="tab:blue")
    plt.title("FFT Magnitude Spectrum")
    plt.xlabel("Frequency")
    plt.ylabel("Magnitude")
    plt.grid(True, linestyle="--", alpha=0.5)

    if output_path:
        plt.savefig(output_path, dpi=200, bbox_inches="tight")
        print(f"Saved plot to: {output_path}")
    else:
        plt.show()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Visualize FFT frequency and magnitude data from a CSV file.")
    parser.add_argument("csv_file", type=str, help="Path to the CSV file containing FFT data.")
    parser.add_argument("-o", "--output", type=str, default=None, help="Optional output image path (PNG, JPG, etc.).")
    args = parser.parse_args()

    try:
        plot_fft(Path(args.csv_file), Path(args.output) if args.output else None)
    except Exception as exc:
        raise SystemExit(f"Error: {exc}")
