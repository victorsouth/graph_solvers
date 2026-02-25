import pandas as pd
import os

# Пути к файлам и папкам
excel_path = 'data_cleaner.xlsx'
raw_folder = 'raw1'
cleaned_folder = 'cleaned'

# Создание папки для вывода, если она не существует
os.makedirs(cleaned_folder, exist_ok=True)

# Загрузка лимитов из Excel
df_limits = pd.read_excel(excel_path, header=None)

file_names_raw = df_limits.iloc[0, 1:].tolist()
max_vals = df_limits.iloc[1, 1:].astype(float).tolist()
min_vals = df_limits.iloc[2, 1:].astype(float).tolist()
max_deltas = df_limits.iloc[3, 1:].astype(float).tolist()

for idx, raw_name in enumerate(file_names_raw):
    print(f"\n📄 Обработка: {raw_name}")

    file_name = raw_name.strip() + '.csv'
    input_path = os.path.join(raw_folder, file_name)

    if not os.path.exists(input_path):
        print(f"❌ Файл не найден: {input_path}")
        continue

    decimal_mark = ','   # десятичный разделитель
    sep_guess = ';'      # разделитель колонок

    try:
        df = pd.read_csv(input_path, header=None, names=['time', 'value'],
                         sep=sep_guess, encoding='cp1251')
    except UnicodeDecodeError as e:
        print(f"❌ Ошибка чтения файла {input_path}: {e}")
        continue

    original_len = len(df)

    # Очистка и преобразование
    df['value'] = (
        df['value']
        .astype(str)
        .str.strip()
        .str.replace(',', '.', regex=False)
    )

    df['value'] = pd.to_numeric(df['value'], errors='coerce')
    df_clean_numeric = df.dropna(subset=['value'])
    non_numeric_removed = original_len - len(df_clean_numeric)

    # Фильтрация по диапазону
    min_val = min_vals[idx]
    max_val = max_vals[idx]
    df_final = df_clean_numeric[
        (df_clean_numeric['value'] >= min_val) &
        (df_clean_numeric['value'] <= max_val)
    ]
    range_removed = len(df_clean_numeric) - len(df_final)
    
    # Фильтрация по максимальной дельте
    max_delta = max_deltas[idx]
    df_final = df_final.reset_index(drop=True)
    diffs = df_final['value'].diff().abs()
    delta_mask = (diffs <= max_delta) | diffs.isna()  # Оставляем первую строку
    df_final = df_final[delta_mask]
    delta_removed = len(diffs) - delta_mask.sum()

    # Подготовка к сохранению
    df_final = df_final.copy()
    df_final['value'] = df_final['value'].astype(str).apply(lambda x: x.replace('.', ','))

    output_path = os.path.join(cleaned_folder, file_name)
    df_final.to_csv(
        output_path,
        index=False,
        header=False,
        sep=sep_guess,
        encoding='utf-8',
        quoting=3  # csv.QUOTE_NONE
    )

    print(f"✅ {input_path} → {output_path}")
    print(f"   Удалено строк со значениями НЕ ЧИСЛО: {non_numeric_removed}")
    print(f"   Удалено строк вне диапазона [{min_val}, {max_val}]: {range_removed}")
    print(f"   Удалено строк по превышению дельты > {max_delta}: {delta_removed}")