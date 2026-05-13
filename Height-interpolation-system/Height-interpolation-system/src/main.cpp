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

//КЛАСС ДЛЯ РАБОТЫ С TIFF-ФАЙЛОМ
class TiffWorking
{
private:
    TIFF* image = nullptr;
    uint32_t imgW, imgH;
    uint16_t typeFormat;
    uint16_t bitPerPixel;

    //Вывод значения в точке по индексу
    float getHeight(void* buf, const int& index)
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
        return -1.0f;
    }

public:
    TiffWorking()
        : imgW(0), imgH(0), typeFormat(0), bitPerPixel(0)
    {
    }

    ~TiffWorking()
    {
        if (image)
            TIFFClose(image);
    }

    //Открытие файла и запись всех данных из него
    bool open(const std::string& fileName)
    {
        image = TIFFOpen(fileName.c_str(), "r");
        if (!image)
            return false;

        //Получаем ширины и высоту изображения
        TIFFGetField(image, TIFFTAG_IMAGELENGTH, &imgH);
        TIFFGetField(image, TIFFTAG_IMAGEWIDTH, &imgW);
        //Получаем формат пикселя высот
        TIFFGetField(image, TIFFTAG_SAMPLEFORMAT, &typeFormat);
        //Количество бит на пиксель
        TIFFGetField(image, TIFFTAG_BITSPERSAMPLE, &bitPerPixel);
        return true;
    }

    //Получения данных из плиточного хранения высот
    bool readTiledData(std::vector<float>& allHeights)
    {
        //Если файл содержит построчное хранения высот
        if (!TIFFIsTiled(image))
            return false;


        //Ширина и высота плиток
        uint32_t tileW, tileH;
        TIFFGetField(image, TIFFTAG_TILEWIDTH, &tileW);
        TIFFGetField(image, TIFFTAG_TILELENGTH, &tileH);
        //Количество плиток в строке
        uint16_t tilesInRow = (imgW + tileW - 1) / tileW;

        //Создаем размер буфера и заполняем буфер значениямм высот из файла
        const tmsize_t bufSize = TIFFTileSize(image);
        void* buf = _TIFFmalloc(bufSize);

        //Количество плиток на изображении
        uint32_t tileQuantity = TIFFNumberOfTiles(image);
        //Кол-во точек в плитке
        uint32_t pointsQuantityInTile = tileW * tileH;

        //Перенос всех высот в массив allHeights
        cout << "[ВЫПОЛНЯЕТСЯ ПЕРЕНОС ВСЕХ ВЫСОТ В 1 ВЕКТОР]" << endl;
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
                uint32_t tx = p - ty * tileW;

                //Глобальные координаты точки на карте
                uint32_t globalX = tx + tileCol * tileW;
                uint32_t globalY = ty + tileRow * tileH;

                //Проверка что глобальная точка не выходит за пределы изображения в пустые занечения плитки
                if (globalX < imgW && globalY < imgH)
                {
                    //Индекс чтобы добавить точку в вектор всех точек в нужное место
                    uint32_t indexInVector = globalY * imgW + globalX;
                    allHeights[indexInVector] = getHeight(buf, p);  //Заполняем высоты
                }
            }
        }
        _TIFFfree(buf); //Освобождаем буфер из памяти
        return true;
    }

    uint32_t getW()
    {
        return imgW;
    }
    uint32_t getH()
    {
        return imgH;
    }
    uint16_t getFormat()
    {
        return typeFormat;
    }
    uint16_t getBPP()
    {
        return bitPerPixel;
    }
};

//КЛАСС С ВЫЧИСЛЕНИЯМИ ИНТЕРПОЛЯЦИЙ
class Interpolator
{
public:
    //Интерполяция по соседям
    static float idw(float x, float y, const std::vector<float>& heights, int imgW, int imgH)
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

        //Ограничиваем координаты, чтобы не выйти за пределы изображения
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
    static float barycentric(float x, float y, const std::vector<float>& heights, int imgW, int imgH)
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
            //Верхний треугольник
            return h_00 + fx * (h_10 - h_00) + fy * (h_01 - h_00);
        else
            //Нижний треугольник
            return h_11 + (1.0f - fx) * (h_01 - h_11) + (1.0f - fy) * (h_10 - h_11);
    }
};

//ОСНОВНОЙ КЛАСС УПРАВЛЕНИЯ
class TerrainManager
{
private:
    std::vector<float> allHeights;
    //Вектор из координат всех точек (0; 0) (0; 1) ..
    std::vector<double> coords;
    //Размер сглаживания интерполяции
    uint32_t scale = 1;
    //Выбор интерполяции
    uint16_t interpolationChoice = 3;

public:
    void run(const std::string& fileName)
    {
        TiffWorking tiff;
        if (!tiff.open(fileName))
        {
            cout << "Файл не удалось открыть! " << endl;
            return;
        }

        cout << "Размер изображения: ";
        cout << tiff.getW() << " x " << tiff.getH() << endl;

        //Заполняем массив ноликами размером с изображение
        allHeights.assign(tiff.getW() * tiff.getH(), 0.0f);
        if (!tiff.readTiledData(allHeights))
        {
            cout << "Устаревший тип! Хранит данные в виде строк, а не плиток!" << endl;
            return;
        }

        getUserSettings();
        processAndSave(tiff.getW(), tiff.getH());
    }

private:
    //Получаем данные от пользователя по поводу интерполяции
    void getUserSettings()
    {
        bool correct = false;
        while (!correct)
        {
            cout << "\nВведите вид интерполяции:"
                "\n1. По соседям"
                "\n2. Барицентральная"
                "\n3. Без интерполяции"
                "\nВведите целое число: ";
            cin >> interpolationChoice;
            if (interpolationChoice > 0 && interpolationChoice <= 3)
            {
                if (interpolationChoice != 3)
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
                {
                    scale = 1;
                    correct = true;
                }
            }
            else
                cout << "Некорректный ввод!!!" << endl;
        }
    }

    void processAndSave(uint32_t imgW, uint32_t imgH)
    {
        //Создаение .obj файла
        std::ofstream file("relief.obj");

        //Новая ширина и высота массива для интерполяции
        uint32_t newW = imgW * scale;
        uint32_t newH = imgH * scale;

        coords.reserve(newW * newH * 2);

        if (interpolationChoice == 3)
            cout << "[ПЕРЕНОС ВЫСОТ В ФАЙЛ OBJ...]" << endl;
        else
            cout << "[ИНТЕРПОЛЯЦИЯ ВЫСОТ И ПЕРЕНОС ВЫСОТ В ФАЙЛ OBJ...]" << endl;

        for (uint32_t y = 0; y < newH; y++)
        {
            for (uint32_t x = 0; x < newW; x++)
            {
                //Высота
                float h;
                //Координаты в исходной сетке
                float origX = (float)x / scale;
                float origY = (float)y / scale;

                //Интерполяция, добавляем высоты в массив
                if (interpolationChoice == 1) //По соседям
                    h = Interpolator::idw(origX, origY, allHeights, imgW, imgH);
                else if (interpolationChoice == 2) //Барацентральная
                    h = Interpolator::barycentric(origX, origY, allHeights, imgW, imgH);
                else //Без интерполяции
                    h = allHeights[y * imgW + x];

                //Записываем вершину в OBJ
                file << "v " << x << " " << h << " " << y << "\n";

                //Координаты для триангуляции
                coords.push_back(x);
                coords.push_back(y);
            }
        }

        cout << "[ВЫПОЛНЯЕТСЯ ПОДСЧЕТ ТРИАНГУЛЯЦИИ...]" << endl;
        //Создаем объект для триангуляции и вектор со всеми треугольниками
        delaunator::Delaunator trangulation(coords);
        auto& trianglesVec = trangulation.triangles;

        cout << "[ВЫПОЛНЯЕТСЯ ПЕРЕНОС ГРАНЕЙ ТРИАНГУЛЯЦИИ В OBJ...]" << endl;
        //Заполение файла obj гранями (треугольники)
        for (int i = 0; i < trianglesVec.size() - 2; i += 3)
            file << "f " << trianglesVec[i] + 1 << " " << trianglesVec[i + 1] + 1 << " " << trianglesVec[i + 2] + 1 << "\n";

        file.close();
        cout << "\nФайл obj с 3д рельефом создан!" << endl;
    }
};

int main()
{
#ifdef _WIN32
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
#endif

    TerrainManager manager;

    std::string fileName;
    cout << "Введите путь до файла относительно проекта: ";
    cin >> fileName;

    manager.run(fileName);

    return 0;
}