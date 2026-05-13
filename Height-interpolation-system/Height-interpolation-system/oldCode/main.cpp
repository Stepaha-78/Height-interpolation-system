#include <iostream>
#include <string>
#include <fstream>

#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#include <tiffio.h>
#include "delaunator.hpp"

using std::cout;
using std::cin;
using std::endl;

//Вывод значения в точке по индексу
float getHeight(void* buf, const int& index, const uint16_t& typeFormat)
{
    //Переводим данные буфера в числа высот под их тип хранения
    if (typeFormat == SAMPLEFORMAT_INT)
    {
        //Указатель на массив из 2 байтных int 
        int16_t* tileHeights = (int16_t*)buf;
        return tileHeights[index];
    }
    else if (typeFormat == SAMPLEFORMAT_UINT)
    {
        //Указатель на массив без знаковых 2 байтных int
        uint16_t* tileHeights = (uint16_t*)buf;
        return tileHeights[index];
    }
    else if (typeFormat == SAMPLEFORMAT_IEEEFP)
    {
        //Указатель на массив из float
        float_t* tileHeights = (float_t*)buf;
        return tileHeights[index];
    }
    else
    {
        return -1.0f;
    }
}

//Интерполяция по соседям
float idw(float x, float y, const std::vector<float>& heights, int imgW, int imgH)
{
    //Находим соседей путём округления вниз и вверх
    int x0 = (int)floor(x);
    int x1 = (int)ceil(x);
    int y0 = (int)floor(y);
    int y1 = (int)ceil(y);

    //Ограничиваем координаты, чтобы не выйти за пределы изображения
    x0 = std::max(0, std::min(x0, imgW - 1));
    x1 = std::max(0, std::min(x1, imgW - 1));
    y0 = std::max(0, std::min(y0, imgH - 1));
    y1 = std::max(0, std::min(y1, imgH - 1));

    //Расстояние до каждого из четырёх соседей
    float d00 = sqrt((x - x0) * (x - x0) + (y - y0) * (y - y0));
    float d10 = sqrt((x - x1) * (x - x1) + (y - y0) * (y - y0));
    float d01 = sqrt((x - x0) * (x - x0) + (y - y1) * (y - y1));
    float d11 = sqrt((x - x1) * (x - x1) + (y - y1) * (y - y1));

    //Если точка точно совпала с пикселем, то выводим его высоту
    if (d00 == 0.0f) 
        return heights[y0 * imgW + x0];
    if (d10 == 0.0f) 
        return heights[y0 * imgW + x1];
    if (d01 == 0.0f) 
        return heights[y1 * imgW + x0];
    if (d11 == 0.0f) 
        return heights[y1 * imgW + x1];

    //Веса соседей, чем ближе сосед, тем больше высота новой точкой будет связана с ним
    float w00 = 1.0f / d00;
    float w10 = 1.0f / d10;
    float w01 = 1.0f / d01;
    float w11 = 1.0f / d11;

    float sumW = w00 + w10 + w01 + w11;

    //Итоговая высота. Взвешенное среднее высот четырёх соседей
    return (w00 * heights[y0 * imgW + x0] + w10 * heights[y0 * imgW + x1] +
        w01 * heights[y1 * imgW + x0] + w11 * heights[y1 * imgW + x1]) / sumW;
}

//Барицентрическая интерполяция
float barycentric(float x, float y, const std::vector<float>& heights, int imgW, int imgH)
{
    int x0 = (int)floor(x);
    int x1 = x0 + 1;
    int y0 = (int)floor(y);
    int y1 = y0 + 1;

    //Ограничиваем координаты, чтобы не выйти за пределы изображения
    x0 = std::max(0, std::min(x0, imgW - 1));
    x1 = std::max(0, std::min(x1, imgW - 1));
    y0 = std::max(0, std::min(y0, imgH - 1));
    y1 = std::max(0, std::min(y1, imgH - 1));

    //Положение внутри клетки
    float fx = x - floor(x);
    float fy = y - floor(y);
    
    //Достаём высоты для 4 точек квадрата
    float h_00 = heights[y0 * imgW + x0];
    float h_10 = heights[y0 * imgW + x1];
    float h_01 = heights[y1 * imgW + x0];
    float h_11 = heights[y1 * imgW + x1];

    //Определяем в каком треугольнике лежит точка
    if (fx + fy <= 1.0f)
    {
        // Верхний треугольник: A=(x0,y0), B=(x1,y0), C=(x0,y1)
        return h_00 + fx * (h_10 - h_00) + fy * (h_01 - h_00);
    }
    else
    {
        // Нижний треугольник: A=(x1,y1), B=(x0,y1), C=(x1,y0)
        return h_11 + (1.0f - fx) * (h_01 - h_11) + (1.0f - fy) * (h_10 - h_11);
    }
}

int main()
{
    #ifdef _WIN32
        SetConsoleCP(65001);
        SetConsoleOutputCP(65001);
    #endif

    
    TIFF* image = TIFFOpen("img/AmerikaLake1.tif", "r");
    if (!image)
    {
        cout << "Файл не удалось открыть" << endl;
        return 1;
    }
    
    //Получаем ширины и высоту изображения
    uint32_t imgW, imgH;
    TIFFGetField(image, TIFFTAG_IMAGELENGTH, &imgH);
    TIFFGetField(image, TIFFTAG_IMAGEWIDTH, &imgW);
    cout << "Размер изображения: ";
    cout << imgW << " x " << imgH << endl;

    //Вывод формата пикселя высот
    uint16_t typeFormat; 
    TIFFGetField(image, TIFFTAG_SAMPLEFORMAT, &typeFormat);
    if (typeFormat == SAMPLEFORMAT_INT)
        cout << "Формат высот int с знаками" << endl;
    else if (typeFormat == SAMPLEFORMAT_UINT)
        cout << "Формат unsigned int без знаков" << endl;
    else
        cout << "Формат float" << endl;

    //Количество бит на пиксель
    uint16_t bitPerPixel;
    if (TIFFGetField(image, TIFFTAG_BITSPERSAMPLE, &bitPerPixel))
        cout << "Количество бит на пиксель фота: " << bitPerPixel << endl;

    //буфер хранения точек
    void* buf = nullptr; 

    //Для плиточного хранения пикселей
    if (TIFFIsTiled(image)) 
    {
        cout << "Изображение хранит данные в виде плиток." << endl;
        
        //Количество плиток на изображении
        uint32_t tileQuantity = TIFFNumberOfTiles(image);
        cout << "Количество плиток: " << tileQuantity << endl;

        //Ширина и высота плиток
        uint32_t tileW, tileH; 
        TIFFGetField(image, TIFFTAG_TILEWIDTH, &tileW);
        TIFFGetField(image, TIFFTAG_TILELENGTH, &tileH);
        cout << "Размер плитки: " << tileW << " x " << tileH << endl;

        //Количество плиток в строке
        uint16_t tilesInRow = (imgW + tileW - 1) / tileW;

        //Создаем размер буфера и заполняем буфер значениямм 0 плитки
        const tmsize_t bufSize = TIFFTileSize(image);
        buf = _TIFFmalloc(bufSize);

        //Создаём массив для всех точек высот изображения
        std::vector<float> allHeights(imgW * imgH, 0.0f);
        cout << "Размер массива всех вершин: " << allHeights.size() << endl;
        cout << "Вес массива: " << ((allHeights.size() * 4)/ (1024)) << " КБ" << endl;
        
        //Вектор из координат всех точек (0; 0) (0; 1) ...
        std::vector<double> coords;
        coords.reserve(imgH*imgW*2);
        
        
        //Размер сглаживания интерполяции
        uint32_t scale = 1;

        //Выбор интерполяции
        bool correct = false;
        uint16_t interpolationChoice;
        while (!correct)
        {
            cout << "\nВведите вид интерполяции:"
                "\n1. По соседям"
                "\n2. Барицентральная"
                "\n3. Без интерполяции"
                "\nВведите целое число: ";
            cin >> interpolationChoice;
            if (interpolationChoice <= 0 || interpolationChoice > 3)
                cout << "Некорректный ввод!!!" << endl;
            else if (interpolationChoice != 3)
            {
                cout << "\nВведите уровень сглаживания интерполяции (целое число от 2, больше 3 не советуется): ";
                cin >> scale;
                if (scale <= 1)
                {
                    cout << "Некорректный ввод!!!" << endl;
                    continue;
                }
                else
                    correct = true;
            }
            else
                correct = true;
        }
        
        //Создаение и заполнение .obj файла
        std::ofstream file("relief.obj");
        if (!file.is_open())
        {
            cout << "Файл не удалось открыть!" << endl;
            return 1;
        }

        uint32_t pointsQuantityInTile = tileW * tileH; //Кол-во точек в плитке
        
        cout << "[ВЫПОЛНЯЕТСЯ ПЕРЕНОС ВСЕХ ВЫСОТ В 1 ВЕКТОР]" << endl;
        //Перенос всех высот в массив allHeights
        for (uint32_t tileNum = 0; tileNum < tileQuantity; tileNum++)
        {
            //Перенос значений высот в буфер tileNum плитки
            TIFFReadEncodedTile(image, tileNum, buf, bufSize);

            //Место текущей плитки в ряду и в столбике
            uint16_t tileRow = tileNum / tilesInRow;
            uint16_t tileCol = tileNum - tileRow * tilesInRow;
            
            //Перебор всех точек в плитке
            for (uint32_t p = 0; p < pointsQuantityInTile; p++)
            {
                //Координаты точки внутри плитки по y и x
                uint32_t ty = p / tileW;
                uint32_t tx = p - ty*tileW;

                //Глобальные координаты точки на карте
                uint32_t globalX = tx + tileCol * tileW;
                uint32_t globalY = ty + tileRow * tileH;

                //Индекс чтобы добавить точку в вектор всех точек в нужное место
                uint32_t indexInVector = globalY * imgW + globalX;
                
                //Проверка что глобальная точка не выходит за пределы изображения в пустые занечения плитки
                if (globalX < imgW && globalY < imgH)
                    allHeights[indexInVector] = getHeight(buf, p, typeFormat); //Заполняем высоты
            }
        }

        if (interpolationChoice == 3)//Если БЕЗ интерполяция записываем точки в OBJ
        {
            cout << "[Перенос высот В ФАЙЛ OBJ...]" << endl;
            for (uint32_t y = 0; y < imgH; y++)
            for (uint32_t x = 0; x < imgW; x++)
            {
                float h = allHeights[y * imgW + x];
                file << "v " << x << " " << h << " " << y << "\n";          

                coords.push_back(x);
                coords.push_back(y);
            }
        }
        else //Если есть интерполяция
        {
            //Новая ширина и высота массива для триангуляции
            uint32_t newW = imgW * scale;
            uint32_t newH = imgH * scale;

            //Отчистка старых координат для триангуляции
            coords.clear();
            coords.reserve(newW * newH * 2);

            //Создание нового вектора высот с интерполяцией
            std::vector<float> newHeights(newW * newH, 0.0f);

            cout << "[Интерполяция высот и перенос высот В ФАЙЛ OBJ...]" << endl;
            for (uint32_t y = 0; y < newH; y++)
                for (uint32_t x = 0; x < newW; x++)
                {
                    //Координаты в исходной сетке
                    float origX = (float)x / scale;
                    float origY = (float)y / scale;

                    //Интерполяция, добавляем высоты в новый массив
                    if (interpolationChoice == 1) //По соседям
                    {
                        float h = idw(origX, origY, allHeights, imgW, imgH);
                        newHeights[y * newW + x] = h;
                        //Записываем вершину в OBJ
                        file << "v " << x << " " << h << " " << y << "\n";
                    }
                    else //Барацентральная
                    {
                        float h = barycentric(origX, origY, allHeights, imgW, imgH);
                        newHeights[y * newW + x] = h;
                        //Записываем вершину в OBJ
                        file << "v " << x << " " << h << " " << y << "\n";
                    }

                    //Координаты для триангуляции
                    coords.push_back(x);
                    coords.push_back(y);
                }
        }

        cout << "[ВЫПОЛНЯЕТСЯ ПОДСЧЕТ ТРИАНГУЛЯЦИИ...]" << endl;
        //Создаем объект для триангуляции и вектор со всеми треугольниками
        delaunator::Delaunator trangulation(coords);
        auto& trianglesVec = trangulation.triangles;
        cout << "Размер вектора триангуляции: " << (trianglesVec.size()*8)/1024 << " КБ" << endl;

        cout << "[ВЫПОЛНЯЕТСЯ ПЕРЕНОС ГРАНЕЙ ТРИАНГУЛЯЦИИ В OBJ...]" << endl;
        //Заполение файла obj гранями (треугольники)
        for (int i = 0; i < trianglesVec.size() - 2; i += 3)
            file << "f " << trianglesVec[i] + 1 << " " << trianglesVec[i + 1] + 1 << " " << trianglesVec[i + 2] + 1 << "\n";
        
        file.close();
        auto min_h = std::min_element(allHeights.begin(), allHeights.end());
        auto max_h = std::max_element(allHeights.begin(), allHeights.end());
        cout << "МИН знач в масиве высот: " << *min_h << "| МАКС знач в масиве высот: " << *max_h << endl;
    }
    //Для построчного хранения (устаревшее)
    else
    {
        cout << "Устаревший тип! Хранит данные в виде строк, а не плиток!" << endl;

        const tmsize_t bufSize = TIFFScanlineSize(image);
        buf = _TIFFmalloc(bufSize);
        TIFFReadScanline(image, buf, 0);
    }

    //Отчищаем буфер, если он не пустой
    if (buf)
    {
        _TIFFfree(buf);
    }

    TIFFClose(image);
    return 0;
}