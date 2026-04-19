#include <iostream>
#include <tiffio.h>

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

    uint16_t format;
    TIFFGetField(image, TIFFTAG_SAMPLEFORMAT, &format);
    if (format == SAMPLEFORMAT_INT)
        cout << "Формат высот int с знаками" << endl;
    else if (format == SAMPLEFORMAT_UINT)
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
        
        const tmsize_t bufSize = TIFFTileSize(image);
        buf = _TIFFmalloc(bufSize);
        TIFFReadEncodedTile(image, 0, buf, bufSize);
        
        int16_t* dataPoints = (int16_t*)buf; //масив из 2 байтных int размером 256х256

        cout << dataPoints[30719] << endl;
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