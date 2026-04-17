#include <iostream>
#include <tiffio.h>

int main()
{
    setlocale(LC_ALL, "ru");
    TIFF* tif = TIFFOpen("src/Amerika1.tif", "r"); //Имя tiff файла
    if (!tif) return 1;

    uint32_t width, height;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

    // Выделяем память под одну строку данных
    tdata_t buf = _TIFFmalloc(TIFFScanlineSize(tif));

    // Читаем первую строку (индекс 0)
    TIFFReadScanline(tif, buf, 0);

    // Преобразуем буфер в массив целых чисел
    int16_t* data = (int16_t*)buf;

    std::cout << "Разрешение: " << width << "x" << height << std::endl;
    std::cout << "Высота в точке (0,0): " << data[0] << " метров" << std::endl;

    _TIFFfree(buf);
    TIFFClose(tif);
}