#include <iostream>
#include "tiffio.h"

int main()
{
    setlocale(LC_ALL, "ru");
    const char* version = TIFFGetVersion();

    if (version)
    {
        std::cout << "LibTIFF успешно подключена!" << std::endl;
        std::cout << "Версия: " << version << std::endl;
    }
    else
    {
        std::cerr << "Ошибка: Не удалось получить данные из библиотеки." << std::endl;
        return 1;
    }
}