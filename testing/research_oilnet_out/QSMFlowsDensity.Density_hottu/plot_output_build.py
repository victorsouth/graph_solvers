from tools.plot_output import plot_output_builder
from pathlib import Path

calc_path = Path(__file__).parent / "output" / "calc"
measure_path = Path(__file__).parent / "output" / "measurements"

plot_output_builder(calc_path, measure_path)