import glob
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path

class calc_timeseries:
    def __init__(self):
        self.status = None
        self.calc_t = None
        self.calc_y = None
        self.measure_t = None
        self.measure_y = None

    def get_confident_data(self):            
        indices = [i for i, s in enumerate(self.status) if s == 2]
        y_measure = [self.measure_y[i] for i in indices]
        y_calc = [self.calc_y[i] for i in indices]
        t = [self.calc_t[i] for i in indices]
        return t, y_calc, y_measure


class comparative_plot:
    @staticmethod
    def load_csvs(path):
        plot_names = glob.glob(str(Path(path) / '*.csv'))
        if str(Path(path) / 'coord_heights.csv') in plot_names:
            plot_names.remove(str(Path(path) / 'coord_heights.csv'))

        rawData = []
        for plot in plot_names:
            df = pd.read_csv(plot, header=0, names=['time', 'value', 'status'],
                             encoding='windows-1251', sep=';')
            df['time'] = pd.to_datetime(df['time'], format="%d.%m.%Y %H:%M:%S")
            rawData.append(df)

        plot_names = [Path(p).stem for p in plot_names]
        return plot_names, rawData

    @staticmethod
    def get_confident_timeseries(plot_names, calc_data, measurement_data):
        result = {}
        for i in range(len(calc_data)):
            calc = calc_data[i]
            measure = measurement_data[i]
            status = calc['status'].tolist()
            if 2 in status:
                ts = calc_timeseries()
                ts.status = status
                ts.calc_t = calc['time'].tolist()
                ts.calc_y = calc['value'].tolist()
                ts.measure_t = measure['time'].tolist()
                ts.measure_y = measure['value'].tolist()
                result[plot_names[i]] = ts
        return result

    def __init__(self, calc_path, measure_path):
        plot_names, calc_data = self.load_csvs(calc_path)
        _, measure_data = self.load_csvs(measure_path)
        self.timeseries = self.get_confident_timeseries(plot_names, calc_data, measure_data)

    def plot(self):
        fig = plt.figure(figsize=(8, 5))
        plotsCount = 3 * len(self.timeseries)
        axes = [plt.subplot(plotsCount, 1, i + 1) for i in range(plotsCount)]

        i = 0
        for plot_name, data in self.timeseries.items():
            axes[i].grid(True)
            axes[i].set_xlabel("Время, ч")
            axes[i].set_ylabel(plot_name)

            t_conf, y_calc_conf, y_measure_conf = data.get_confident_data()
            axes[i].plot(data.calc_t, data.calc_y, 'ro', markersize=1, label='Недостоверный расчет')
            axes[i].plot(t_conf, y_calc_conf, 'go', markersize=1, label='Расчет')
            axes[i].plot(data.measure_t, data.measure_y, 'bo', markersize=1, label='Измерения')
            axes[i].legend(loc='upper left')

            e = [a - b for a, b in zip(data.measure_y, data.calc_y)]
            e_conf = [a - b for a, b in zip(y_measure_conf, y_calc_conf)]

            axes[i+1].grid(True)
            axes[i+1].plot(data.calc_t, e, 'ro', markersize=1)
            axes[i+1].plot(t_conf, e_conf, 'go', markersize=1)

            axes[i+2].hist(e_conf, cumulative=False, density=True)
            axes[i+2].set_ylabel('Гистограмма')

            i += 3

        plt.subplots_adjust(left=0.06, bottom=0.06, right=0.971, top=0.973,
                            wspace=0.2, hspace=0.24)
        plt.show()


def plot_output_builder(calc_path, measure_path):
    plotter = comparative_plot(calc_path, measure_path)
    plotter.plot()
