Import("env")
import os

# Получаем имя текущего окружения (например, module_soilMoisture или host)
env_name = env.get("PIOENV")

# Формируем путь: src/<имя_env>/data
custom_data_dir = os.path.join(env.get("PROJECT_DIR"), "src", env_name, "data")

if os.path.exists(custom_data_dir):
    # Переопределяем встроенную переменную PlatformIO
    env.Replace(PROJECT_DATA_DIR=custom_data_dir)