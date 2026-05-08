#include <iostream>
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
        coords.reserve(imgH*imgW);

        //Создаение и заполнение .obj файла
        std::ofstream file("relief.obj");
        if (!file.is_open())
        {
            cout << "Файл не удалось открыть!" << endl;
            return 1;
        }

        uint32_t pointsQuantityInTile = tileW * tileH; //Кол-во точек в плитке

        cout << "[ВЫПОЛНЯЕТСЯ ПЕРЕНОС ВСЕХ ВЫСОТ В 1 ВЕКТОР И В ФАЙЛ OBJ...]" << endl;
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
                {
                    allHeights[indexInVector] = getHeight(buf, p, typeFormat);

                    coords.push_back(globalX);
                    coords.push_back(globalY);

                    file << "v " << globalX << " " << allHeights[indexInVector] << " " << globalY << "\n";
                }
            }
        }
        
        cout << "[ВЫПОЛНЯЕТСЯ ПОДСЧЕТ ТРИАНГУЛЯЦИИ...]" << endl;
        //Создаем объект для триангуляции и вектор со всеми треугольниками
        delaunator::Delaunator trangulation(coords);
        auto& trianglesVec = trangulation.triangles;
        cout << "Размер вектора триангуляции: " << (trianglesVec.size()*8)/1024 << " КБ" << endl;

        cout << "[ВЫПОЛНЯЕТСЯ ПЕРЕНОС ГРАНЕЙ В ТРИАНГУЛЯЦИИ В OBJ...]" << endl;
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
        cout << "Хранит данные в виде строк" << endl;

        const tmsize_t bufSize = TIFFScanlineSize(image);
        buf = _TIFFmalloc(bufSize);
        TIFFReadScanline(image, buf, 0); //1-ую строчку(нулевую)
    }

    //Отчищаем буфер, если он не пустой
    if (buf)
    {
        _TIFFfree(buf);
    }

    TIFFClose(image);
    return 0;
}