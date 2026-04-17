#include <iostream>
#include <tiffio.h>

int main()
{
    setlocale(LC_ALL, "ru");

    TIFF* tif = TIFFOpen("src/Amerika1.tif", "r");
    if (!tif)
    {
        std::cerr << "Ошибка: Не удалось открыть файл Kavkaz1.tif!" << std::endl;
        return 1;
    }

    uint32_t width, height;
    uint16_t sampleFormat, bitsPerSample;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);

    std::cout << "Разрешение: " << width << "x" << height << " пикселей" << std::endl;
    std::cout << "Глубина: " << bitsPerSample << " бит" << std::endl;

    //Буфер для данных
    int16_t* data = nullptr;

    if (TIFFIsTiled(tif))
    {
        std::cout << "Структура: Tiled" << std::endl;

        tmsize_t tileSize = TIFFTileSize(tif);
        tdata_t tileBuf = _TIFFmalloc(tileSize);

        //Читаею самый первый тайл
        if (TIFFReadEncodedTile(tif, 0, tileBuf, tileSize) > 0)
        {
            data = (int16_t*)tileBuf;
            std::cout << "Высота в точке (0,0): " << data[0] << " м." << std::endl;
            std::cout << "Высота в точке (1,0): " << data[1] << " м." << std::endl;
            std::cout << "Высота в точке (2,0): " << data[2] << " м." << std::endl;
        }

        _TIFFfree(tileBuf);
    }
    else
    {
        std::cout << "Структура: Scanline" << std::endl;

        tmsize_t scanlineSize = TIFFScanlineSize(tif);
        tdata_t scanlineBuf = _TIFFmalloc(scanlineSize);

        // Читаем первую строку
        if (TIFFReadScanline(tif, scanlineBuf, 0) > 0)
        {
            data = (int16_t*)scanlineBuf;
            std::cout << "Высота в точке (0,0): " << data[0] << " м." << std::endl;
        }

        _TIFFfree(scanlineBuf);
    }

    TIFFClose(tif);

    std::cin.get();
    return 0;
}