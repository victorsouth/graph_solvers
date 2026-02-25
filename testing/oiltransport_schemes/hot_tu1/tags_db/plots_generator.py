import os
import pandas as pd
import matplotlib.pyplot as plt

# Папки
input_folder = 'raw1'
plot_folder = 'plots1'
os.makedirs(plot_folder, exist_ok=True)

for file in os.listdir(input_folder):
    if file.endswith('.csv'):
        file_path = os.path.join(input_folder, file)
        print(f"📊 Обработка: {file}")

        try:
            # Чтение CSV
            df = pd.read_csv(file_path, sep=';', header=None, names=['Datetime', 'value'], encoding='utf-8')

            # Преобразуем дату/время
            df['Datetime'] = pd.to_datetime(df['Datetime'], dayfirst=True, errors='coerce')

            # Преобразуем value в число 
            df['value'] = pd.to_numeric(df['value'].astype(str).str.replace(',', '.'), errors='coerce')

            # Установка индекса времени
            df.set_index('Datetime', inplace=True)

            # Построение графика
            plt.figure(figsize=(12, 6))
            plt.plot(df.index, df['value'], label='Value', color='blue', linewidth=1)
            plt.title(f'График: {file}', fontsize=14)
            plt.xlabel('Время', fontsize=12)
            plt.ylabel('Значение', fontsize=12)
            plt.grid(True)
            plt.tight_layout()

            # Сохранение графика
            plot_name = os.path.splitext(file)[0] + '.png'
            plt.savefig(os.path.join(plot_folder, plot_name))
            plt.close()

            print(f"✅ Сохранён: {plot_name}")

        except Exception as e:
            print(f"❌ Ошибка: {file} — {e}")
