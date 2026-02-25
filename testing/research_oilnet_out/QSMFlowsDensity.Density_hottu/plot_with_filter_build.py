from tools.plot_with_filter import plot_with_filter_builder
from pathlib import Path

calc_path = Path(__file__).parent / "output" / "calc"
measure_path = Path(__file__).parent / "output" / "measurements"

plot_with_filter_builder(calc_path, measure_path)