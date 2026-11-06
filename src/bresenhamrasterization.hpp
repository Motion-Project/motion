#include <functional>

namespace bresenhamrasterization
{
    void plotLine(int x0, int y0, int x1, int y1, std::function<void(int,int)> setPixel);
    void plotLineAA(int x0, int y0, int x1, int y1, std::function<void(int, int, long)> setPixelAA);
    void plotLineWidth(int x0, int y0, int x1, int y1, float wd, std::function<void(int, int, long)> setPixelAA);
}