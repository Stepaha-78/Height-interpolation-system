#include <iostream>
#include <tiffio.h>
#include "delaunator.hpp"

using std::cout;
using std::cin;
using std::endl;

int main()
{
    setlocale(LC_ALL, "ru");

    TIFF* image = TIFFOpen("src/AmerikaLake1.tif", "r");
    if (!image)
    {
        cout << "Файл не удалось открыть" << endl;
        return 1;
    }


    uint32_t imgWidth, imgHeight;
    TIFFGetField(image, TIFFTAG_IMAGELENGTH, &imgHeight); 
    TIFFGetField(image, TIFFTAG_IMAGEWIDTH, &imgWidth);
    cout << "Размер изображения: ";
    cout << imgWidth << " x " << imgHeight << endl;

    uint16_t typeFormat;
    TIFFGetField(image, TIFFTAG_SAMPLEFORMAT, &typeFormat);
    if (typeFormat == SAMPLEFORMAT_INT)
        cout << "Формат высот int с знаками" << endl;
    else if (typeFormat == SAMPLEFORMAT_UINT)
        cout << "Формат unsigned int без знаков" << endl;
    else
        cout << "Формат float" << endl;

    uint16_t bitPerPixel;
    if (TIFFGetField(image, TIFFTAG_BITSPERSAMPLE, &bitPerPixel))
        cout << "Количество бит на пиксель фота: " << bitPerPixel << endl;

    
    void* buf = nullptr; //буфер хранения точек

    if (TIFFIsTiled(image)) //Это плиточное хранение?
    {
        cout << "Хранит данные в виде плиток." << endl;
        
        uint32_t numTiles = TIFFNumberOfTiles(image);
        cout << "Количество плиток: " << numTiles << endl;

        uint32_t tileW, tileH; //Ширина и высота плиток
        TIFFGetField(image, TIFFTAG_TILEWIDTH, &tileW); 
        TIFFGetField(image, TIFFTAG_TILELENGTH, &tileH);
        cout << "Размер плитки" << tileW << " x " << tileH << endl;\

        const tmsize_t bufSize = TIFFTileSize(image);
        buf = _TIFFmalloc(bufSize);
        TIFFReadEncodedTile(image, 0, buf, bufSize);

        
        if (typeFormat == SAMPLEFORMAT_INT)
        {
            int16_t* dataPoints = (int16_t*)buf; //масив из 2 байтных int размером 256х256
            cout << dataPoints[0] << " метров" << endl;
        }
        else if (typeFormat == SAMPLEFORMAT_UINT)
        {
            uint16_t* dataPoints = (uint16_t*)buf; //масив из 2 байтных без знаковых int размером 256х256
            cout << dataPoints[0] << " метров" << endl;
        }
        else if (typeFormat == SAMPLEFORMAT_IEEEFP)
        {
            float_t* dataPoints = (float_t*)buf; //масив из 4 байтных float размером 256х256
            cout << dataPoints[0] << " метров" << endl;
        }
    }
    else
    {
        cout << "Хранит данные в виде строк" << endl;

        const tmsize_t bufSize = TIFFScanlineSize(image);
        buf = _TIFFmalloc(bufSize);
        TIFFReadScanline(image, buf, 0); //1-ую строчку(нулевую)
    }
    

    if (buf)
    {
        _TIFFfree(buf);
    }
    
    TIFFClose(image);
    return 0;
}