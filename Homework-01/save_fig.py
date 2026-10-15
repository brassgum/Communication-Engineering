import pandas as pd
import matplotlib.pyplot as plt
import sys

def plot_csv(path):
	df = pd.read_csv(path)

	if len(df.columns) < 2:
		raise ValueError("CSV must contain at least 2 columns")

	x = df.iloc[:, 0]
	y = df.iloc[:, 1]

	plt.figure(figsize=(12, 6))
	plt.plot(x, y)

	plt.xlabel(df.columns[0])
	plt.ylabel(df.columns[1])
	plt.title(f"{path} : {df.columns[1]} vs {df.columns[0]}")

	plt.grid(True)
	plt.tight_layout()
#	plt.show()
	plt.savefig(path + '.png', dpi = 300, bbox_inches = 'tight')
	plt.close()

if __name__ == "__main__":
	if len(sys.argv) >= 2:
		plot_csv(sys.argv[1])				
	else:
		print("Usage: python3 visualize.py <csv_file>")
	
