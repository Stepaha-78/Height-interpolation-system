#include <iostream>

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
        uint32_t tileNum = TIFFNumberOfTiles(image);
        cout << "Количество плиток: " << tileNum << endl;

        //Ширина и высота плиток
        uint32_t tileW, tileH; 
        TIFFGetField(image, TIFFTAG_TILEWIDTH, &tileW);
        TIFFGetField(image, TIFFTAG_TILELENGTH, &tileH);
        cout << "Размер плитки: " << tileW << " x " << tileH << endl;

        //Создаем размер буфера и заполняем буфер значениямм 0 плитки
        const tmsize_t bufSize = TIFFTileSize(image);
        buf = _TIFFmalloc(bufSize);
        TIFFReadEncodedTile(image, 0, buf, bufSize);

        //Создаём массив для всех точек высот изображения
        std::vector<float> allHeights(imgW * imgH, 0.0f);
        cout << "Размер массива всех вершин: " << allHeights.size() << endl;
        cout << "Вес массива: " << ((allHeights.size() * 4)/ (1024)) << " килобайт" << endl;
            
        //Переводим данные буфера в числа высот под их тип хранения
        if (typeFormat == SAMPLEFORMAT_INT)
        {
            //масив из 2 байтных int размером 256х256
            int16_t* dataPoints = (int16_t*)buf;
            cout << dataPoints[0] << " метров" << endl;
            allHeights[0] = dataPoints[0];
        }
        else if (typeFormat == SAMPLEFORMAT_UINT)
        {
            //масив из 2 байтных без знаковых int размером 256х256
            uint16_t* dataPoints = (uint16_t*)buf;
            cout << dataPoints[0] << " метров" << endl;
        }
        else if (typeFormat == SAMPLEFORMAT_IEEEFP)
        {
            //масив из 4 байтных float размером 256х256
            float_t* dataPoints = (float_t*)buf;
            cout << dataPoints[0] << " метров" << endl;
        }

        cout << allHeights[0] << endl;
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