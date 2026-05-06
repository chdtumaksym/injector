cmake_minimum_required(VERSION 3.10)
project(FermProject)

set(CMAKE_CXX_STANDARD 17)
# Отключаем защиты стека и оптимизацию для стабильности шелл-кода
set(CMAKE_CXX_FLAGS_RELEASE "/Od /GS- /MD /INCREMENTAL:NO")

# Создаем инжектор из main.cpp
add_executable(injector main.cpp)

# Создаем DLL из my_cheat.cpp
add_library(my_cheat SHARED my_cheat.cpp)
