#!/usr/bin/env python3
import json
import os
import sys
import shutil
from pathlib import Path

def find_nearest_cpp(header_path, compile_commands):
    """Находит ближайший .cpp файл для header-файла"""
    # Нормализуем путь header-файла
    header_path_normalized = header_path.replace('\\', '/')
    header_dir = Path(header_path_normalized).parent
    header_name = Path(header_path_normalized).name
    
    # Ищем .cpp файл в той же директории
    for entry in compile_commands:
        cpp_file_str = entry['file'].replace('\\', '/')
        cpp_file = Path(cpp_file_str)
        if cpp_file.suffix == '.cpp':
            cpp_dir = cpp_file.parent
            # Сравниваем нормализованные пути
            if str(cpp_dir).replace('\\', '/') == str(header_dir).replace('\\', '/'):
                return entry
    
    # Ищем .cpp файл в родительской директории
    parent_dir = header_dir.parent
    for entry in compile_commands:
        cpp_file_str = entry['file'].replace('\\', '/')
        cpp_file = Path(cpp_file_str)
        if cpp_file.suffix == '.cpp':
            cpp_dir = cpp_file.parent
            if str(cpp_dir).replace('\\', '/') == str(parent_dir).replace('\\', '/'):
                return entry
    
    # Ищем любой .cpp файл из той же библиотеки
    for entry in compile_commands:
        cpp_file_str = entry['file'].replace('\\', '/')
        cpp_file = Path(cpp_file_str)
        if cpp_file.suffix == '.cpp':
            return entry
    
    return None

def find_all_header_files(project_root):
    """Автоматически находит все header-файлы в проекте"""
    project_path = Path(project_root)
    header_files = []
    
    # Ищем все .h и .hpp файлы в src/ и testing/
    search_dirs = ['src', 'testing']
    for search_dir in search_dirs:
        search_path = project_path / search_dir
        if search_path.exists():
            for pattern in ['**/*.h', '**/*.hpp']:
                for header_file in search_path.glob(pattern):
                    # Пропускаем файлы в out/, build/, .git/ и других служебных директориях
                    header_str = str(header_file)
                    if any(part in header_str for part in ['/out/', '/build/', '/.git/', '/.vscode/']):
                        continue
                    header_files.append(header_str)
    
    return sorted(header_files)

def add_headers_to_compile_commands(compile_commands_path, header_files, root_compile_commands_path=None):
    """Добавляет записи для header-файлов в compile_commands.json"""
    if not os.path.exists(compile_commands_path):
        print(f"Warning: {compile_commands_path} not found")
        return
    
    with open(compile_commands_path, 'r', encoding='utf-8') as f:
        compile_commands = json.load(f)
    
    added_count = 0
    for header_file in header_files:
        header_path = Path(header_file).resolve()
        if not header_path.exists():
            continue
        
        # Нормализуем путь: используем прямые слэши, как в остальных записях compile_commands.json
        header_str = str(header_path).replace('\\', '/')
        
        # Проверяем, есть ли уже запись для этого header
        existing_entry = None
        for i, entry in enumerate(compile_commands):
            # Нормализуем путь из записи для сравнения
            entry_file = entry['file'].replace('\\', '/')
            if entry_file == header_str:
                existing_entry = i
                break
        
        # Если запись уже есть, обновляем её вместо пропуска
        if existing_entry is not None:
            # Удаляем старую запись, чтобы добавить обновленную
            compile_commands.pop(existing_entry)
        
        # Находим ближайший .cpp файл
        nearest_cpp = find_nearest_cpp(header_str, compile_commands)
        if nearest_cpp:
            # Создаем запись для header на основе .cpp файла
            header_entry = nearest_cpp.copy()
            header_entry['file'] = header_str
            # Меняем команду компиляции для header-файла
            command = header_entry['command']
            # Заменяем путь к .cpp файлу на путь к header
            # Нормализуем путь к .cpp файлу для замены
            cpp_path = Path(nearest_cpp['file']).resolve()
            cpp_path_str = str(cpp_path).replace('\\', '/')
            # Заменяем все варианты пути (с обратными и прямыми слэшами)
            command = command.replace(str(cpp_path), header_str)
            command = command.replace(cpp_path_str, header_str)
            # Для header-файлов в oil_transport и testing добавляем автоматическое включение oil_transport.h
            # Это позволяет clangd видеть зависимости через централизованный header
            # Проверяем, что это header-файл в директории oil_transport или testing
            needs_include = (('oil_transport' in header_str or 'testing' in header_str) and 
                           (header_str.endswith('.h') or header_str.endswith('.hpp')))
            
            # Добавляем флаг для компиляции header-файла
            if needs_include and '-include' not in command:
                # Добавляем флаг -include перед -c
                command = command.replace(' -c ', ' -include oil_transport.h -c ')
            command = command.replace('-c ', '-c -x c++-header ')
            
            header_entry['command'] = command
            compile_commands.append(header_entry)
            added_count += 1
    
    if added_count > 0:
        with open(compile_commands_path, 'w', encoding='utf-8') as f:
            json.dump(compile_commands, f, indent=2, ensure_ascii=False)
        print(f"Added {added_count} header files to compile_commands.json")
        
        # Копируем обновленный файл в корень проекта, если указан путь
        if root_compile_commands_path and root_compile_commands_path != compile_commands_path:
            try:
                shutil.copy2(compile_commands_path, root_compile_commands_path)
                print(f"Copied compile_commands.json to {root_compile_commands_path}")
            except Exception as e:
                print(f"Warning: Failed to copy to root: {e}")
    else:
        print("No new header files to add")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python add_headers_to_compile_commands.py <compile_commands.json> [root_compile_commands.json] [header1] [header2] ...")
        print("       If no headers specified, will auto-discover all .h/.hpp files in src/ and testing/")
        sys.exit(1)
    
    compile_commands_path = sys.argv[1]
    
    # Проверяем, есть ли путь к корневому compile_commands.json
    root_compile_commands_path = None
    header_files = []
    
    if len(sys.argv) >= 3:
        # Если второй аргумент похож на путь к файлу (содержит .json), это корневой путь
        if sys.argv[2].endswith('.json') and sys.argv[2] != compile_commands_path:
            root_compile_commands_path = sys.argv[2]
            header_files = sys.argv[3:]
        else:
            header_files = sys.argv[2:]
    
    # Если header-файлы не указаны, находим их автоматически
    if not header_files:
        # Определяем корневую директорию проекта (где находится .vscode)
        script_dir = Path(__file__).parent
        project_root = script_dir.parent
        
        # Находим все header-файлы
        header_files = find_all_header_files(project_root)
        print(f"Auto-discovered {len(header_files)} header files")
    
    add_headers_to_compile_commands(compile_commands_path, header_files, root_compile_commands_path)

