import os
import pandas as pd
import matplotlib.pyplot as plt

# Папка с данными
input_folder = 'cleaned'

# Подготовка фигуры с двумя осями
fig, (ax_rho, ax_q) = plt.subplots(2, 1, figsize=(14, 8), sharex=True)

# Перебираем файлы
for file in sorted(os.listdir(input_folder)):
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

            # Убираем строки с NaN
            df.dropna(subset=['Datetime', 'value'], inplace=True)

            # Построение на соответствующей оси
            if file.startswith('rho'):
                ax_rho.plot(df['Datetime'], df['value'], label=file, linewidth=1)
            elif file.startswith('q'):
                ax_q.plot(df['Datetime'], df['value'], label=file, linewidth=1)

        except Exception as e:
            print(f"❌ Ошибка: {file} — {e}")

# Настройка верхнего графика
ax_rho.set_title('Тренды RHO', fontsize=14)
ax_rho.set_ylabel('Значение', fontsize=12)
ax_rho.grid(True)
ax_rho.legend(fontsize=8)

# Настройка нижнего графика
ax_q.set_title('Тренды Q', fontsize=14)
ax_q.set_xlabel('Время', fontsize=12)
ax_q.set_ylabel('Значение', fontsize=12)
ax_q.grid(True)
ax_q.legend(fontsize=8)

plt.tight_layout()
plt.show()