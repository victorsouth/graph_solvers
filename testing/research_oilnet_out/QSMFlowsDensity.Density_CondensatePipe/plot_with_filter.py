import glob
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import os
from pathlib import Path
from collections import namedtuple
from scipy.signal import correlate

def load_csvs(path):
    plot_names = glob.glob(path + '/*.csv')
    if 'coord_heights.csv' in plot_names:
        plot_names.remove('coord_heights.csv')

    rawData = list()
    for plot in plot_names:
        df = pd.read_csv(f'{plot}', header = 0, names = ['time', 'value', 'status'], encoding='windows-1251', sep=';')
        df['time'] = pd.to_datetime(df['time'].values, format = "%d.%m.%Y %H:%M:%S")
        #df['value'] = df['value'].str.replace(',', '.').astype(float)

        rawData.append(df)

    for i in range(len(plot_names)):
        plot_names[i] = Path(plot_names[i]).stem

    return plot_names, rawData


def plot_timeseries(plot_names, rawData):
    plotsCount = len(plot_names) 

    fig = plt.figure(figsize=(8, 5))

    axes = [plt.subplot(plotsCount, 1, _ + 1) for _ in range(plotsCount)]

    plots = list()
    for i in range(len(axes)) :
        axes[i].clear()
        axes[i].grid(visible = True)
        axes[i].set_xlabel("Время, ч")
        axes[i].set_ylabel(plot_names[i])
        data = rawData[i]
        t = data['time'].tolist()
        x = data['value'].tolist()

        status = data['status'].tolist()
        indices = list(filter(lambda i: status[i] == 2, range(len(status))))

        x_conf = [x[i] for i in indices]
        t_conf = [t[i] for i in indices]

        axes[i].plot(t, x, 'ro', markersize = 1)
        axes[i].plot(t_conf, x_conf, 'go', markersize = 1)

    plt.subplots_adjust(left=0.06, bottom=0.06, right=0.971, top=0.973, 
            wspace=0.2, hspace=0.24)

    plt.show()

class calc_timeseries:
    def __init__(self):
        self.status = None
        self.calc_t = None
        self.calc_y = None
        self.measure_t = None
        self.measure_y = None
    def get_confident_data(self):            
        indices = list(filter(lambda i: self.status[i] == 2, range(len(self.status))))

        y_measure = [self.measure_y[i] for i in indices]
        y_calc = [self.calc_y[i] for i in indices]
         # здесь бы проверку, что calc_t и calc_y совпадают... 
        t = [self.calc_t[i] for i in indices]
        return t, y_calc, y_measure

class comparative_plot:
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



    def __init__(self, plot_names, calc_data, measurement_data):
        self.timeseries = self.get_confident_timeseries(plot_names, calc_data, measurement_data)
    
    
    
    def plot(self):
        # создаем оси, столько сколько данных в self.timeseries
        fig = plt.figure(figsize=(8, 5))
        plotsCount = 3 * len(self.timeseries)
        axes = [plt.subplot(plotsCount, 1, _ + 1) for _ in range(plotsCount)]

        # вывод данных
        i = 0
        for plot_name, data in self.timeseries.items():
            axes[i].clear()
            axes[i].grid(visible = True)
            axes[i].set_xlabel("Время, ч")
            axes[i].set_ylabel(plot_name)

            # далее суффикс _conf от confident - достоверный
            t_conf, y_calc_conf, y_measure_conf = data.get_confident_data()

            # График расчетных данных и измерений
            axes[i].plot(data.calc_t, data.calc_y, 'ro', markersize = 1, label = 'Недостоверный расчет')
            y_calc_conf_line, = axes[i].plot(t_conf, y_calc_conf, 'go', markersize = 1, label = 'Расчет')
            axes[i].plot(data.measure_t, data.measure_y, 'bo', markersize = 1, label = 'Измерения')

            axes[i].legend(loc='upper left', numpoints=10) 

            
            e = [a - b for a, b in zip(data.measure_y, data.calc_y)]
            e_conf = [a - b for a, b in zip(y_measure_conf, y_calc_conf)]

            # График разности расчетных данных и измерений
            axes[i+1].grid(visible = True)
            axes[i+1].set_xlabel("Время, ч")
            axes[i+1].set_ylabel(plot_name)
            axes[i+1].plot(data.calc_t, e, 'ro', markersize = 1)
            axes[i+1].plot(t_conf, e_conf, 'go', markersize = 1)

            axes[i+2].hist(e_conf, cumulative=False, density=True)
            axes[i+2].set_ylabel('Гистограмма')
            i = i + 3

        plt.subplots_adjust(left=0.06, bottom=0.06, right=0.971, top=0.973, 
                wspace=0.2, hspace=0.24)
        plt.legend()
        plt.show(block=False)

    def plot_filtered(self, alpha):
        series_count = len(self.timeseries)
        fig, axes = plt.subplots(series_count, 1, figsize=(8, 3 * series_count), sharex=True)

        if series_count == 1:
            axes = [axes]

        for ax, (plot_name, data) in zip(axes, self.timeseries.items()):
            t_conf, y_calc_conf, y_measure_conf = data.get_confident_data()

            if not t_conf:  # если нет достоверных точек
                ax.set_title(f"{plot_name} — нет достоверных данных")
                ax.grid(True)
                continue

            # Экспоненциальное сглаживание
            y_filt = [y_calc_conf[0]]
            for k in range(1, len(y_calc_conf)):
                y_filt.append(alpha * y_calc_conf[k] + (1 - alpha) * y_filt[-1])

            ax.plot(t_conf, y_calc_conf, 'go', markersize=1, label='Достоверный расчет')
            ax.plot(t_conf, y_filt, 'm-', linewidth=1.2, label='Сглаженный расчет')
            ax.plot(t_conf, y_measure_conf, 'bo', markersize=1, label='Измерения')

            ax.set_ylabel(plot_name)
            ax.grid(True)
            ax.legend(loc='upper left')

        plt.xlabel("Время, ч")
        plt.suptitle("Достоверные расчеты с экспоненциальным фильтром", y=0.005)
        plt.tight_layout()
        plt.show(block=False)
    
    def plot_sma(self, window_minutes=2160):
        import numpy as np
        import pandas as pd
        import matplotlib.pyplot as plt

        for plot_name, data in self.timeseries.items():
            # --- Получаем данные ---
            t_conf, y_calc_conf, y_measure_conf = data.get_confident_data()
            t_conf = np.array(t_conf)
            y_calc_conf = np.array(y_calc_conf)
            y_measure_conf = np.array(y_measure_conf)

            # --- Скользящее среднее ---
            y_series = pd.Series(y_calc_conf, index=pd.to_datetime(t_conf))
            y_sma = y_series.rolling(window=window_minutes, center=True).mean()

            # --- Убираем NaN ---
            mask = ~y_sma.isna()
            t_clean = t_conf[mask]
            y_calc_clean = y_calc_conf[mask]
            y_measure_clean = y_measure_conf[mask]
            y_sma_clean = y_sma.values[mask]

            # --- Создаем фигуру с 4 подграфиками ---
            fig, axes = plt.subplots(4, 1, figsize=(14, 16))

            # --- 1. Исходный график ---
            axes[0].plot(t_clean, y_calc_clean, 'purple', markersize=1, label=f'{plot_name} (достоверный расчет)')
            axes[0].plot(t_clean, y_sma_clean, 'red', linewidth=2, label=f'{plot_name} (SMA)')
            axes[0].plot(t_clean, y_measure_clean, 'b.', markersize=1, label=f'{plot_name} (измерения)')
            axes[0].set_ylabel("Скользящее среднее")
            axes[0].legend()
            axes[0].grid(True)

            # --- 2. Кросс-корреляция приращений ---
            del_y_sma = np.diff(y_sma_clean)
            del_y_measure = np.diff(y_measure_clean)

            r = np.correlate(del_y_sma - np.mean(del_y_sma),
                            del_y_measure - np.mean(del_y_measure),
                            mode='full')
            r /= (np.std(del_y_sma) * np.std(del_y_measure) * len(del_y_sma))

            lags = np.arange(-len(del_y_sma) + 1, len(del_y_sma))
            I = np.argmax(np.abs(r))
            lag_at_max = lags[I]
            print(f"[{plot_name}] Максимальная корреляция при лаге {lag_at_max}: {r[I]:.4f}")

            axes[1].plot(lags, r, label='Кросс-корреляция приращений')
            axes[1].axvline(lag_at_max, color='r', linestyle='--', label=f'Макс лаг={lag_at_max}')
            axes[1].set_ylabel("Корреляция")
            axes[1].grid(True)
            axes[1].legend()

            # --- 3. Сдвинутый SMA ---
            y_sma_shifted = np.roll(y_sma_clean, -lag_at_max)

            if lag_at_max > 0:
                t_shifted = t_clean[:-lag_at_max]
                y_sma_shifted = y_sma_shifted[:-lag_at_max]
            elif lag_at_max < 0:
                t_shifted = t_clean[-lag_at_max:]
                y_sma_shifted = y_sma_shifted[-lag_at_max:]
            else:
                t_shifted = t_clean
                y_sma_shifted = y_sma_shifted

            # --- Сдвиг по оси Y ---
            y_measure_segment = y_measure_clean[:len(t_shifted)]
            y_shift_correction = np.mean(y_measure_segment) - np.mean(y_sma_shifted)
            y_sma_shifted_corrected = y_sma_shifted + y_shift_correction

            axes[2].plot(t_shifted, y_sma_shifted_corrected, 'r-', linewidth=1.5,
                        label=f'SMA сдвинуто по X на {lag_at_max} итерац. и по Y на {y_shift_correction:.2f}')
            axes[2].plot(t_shifted, y_measure_segment, 'b.', markersize=1, label='Измерения')
            axes[2].set_ylabel("Сдвинутое SMA")
            axes[2].legend()
            axes[2].grid(True)
            axes[2].set_xlabel("Время")

            # --- 4. Гистограмма ошибок ---
            errors = y_sma_shifted_corrected - y_measure_segment
            axes[3].hist(errors, bins=50, color='gray', edgecolor='black')
            axes[3].set_title(f'Гистограмма ошибок ({plot_name})')
            axes[3].set_xlabel('Ошибка (SMA - измерения)')
            axes[3].set_ylabel('Частота')
            axes[3].grid(True)

            plt.tight_layout()
            plt.show(block=True)

plot_names, calc_result = load_csvs('output/calc')
plot_names, measure_result = load_csvs('output/measurements')

plotter = comparative_plot(plot_names, calc_result, measure_result)
plotter.plot()
plotter.plot_filtered(alpha=0.001)
plotter.plot_sma()

dummy = 1