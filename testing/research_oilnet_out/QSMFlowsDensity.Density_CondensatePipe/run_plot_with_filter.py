from tools.plot_with_filter import run
from pathlib import Path

calc_path = Path(__file__).parent / "output" / "calc"
measure_path = Path(__file__).parent / "output" / "measurements"

run(calc_path, measure_path)