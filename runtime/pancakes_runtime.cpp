#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern "C"
{
    int _fltused{};

    HANDLE hStdin{};
    HANDLE hStdout{};

    void pancakes_init()
    {
        hStdin = GetStdHandle(STD_INPUT_HANDLE);
        hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    float pancakes_parse_float(char const* str, int const len)
    {
        float res{};
        float div{1.0f};
        bool neg{};
        bool dot{};
        int i{};
        if (len > 0 && str[0] == '-')
        {
            neg = true;
            i = 1;
        }
        for (; i < len; i++)
        {
            if (str[i] == '.')
            {
                dot = true;
                continue;
            }
            if (str[i] < '0' || str[i] > '9')
                break;
            if (!dot)
                res = res * 10.0f + static_cast<float>(str[i] - '0');
            else
            {
                div *= 10.0f;
                res += static_cast<float>(str[i] - '0') / div;
            }
        }
        return neg ? -res : res;
    }

    void pancakes_print_float(float val)
    {
        char buf[64];
        char temp[32];
        int i{};
        if (val < 0)
        {
            buf[i++] = '-';
            val = -val;
        }
        int intPart{};
        intPart = static_cast<int>(val);
        float fracPart{};
        fracPart = val - static_cast<float>(intPart);
        int j{};
        if (intPart == 0)
            temp[j++] = '0';

        while (intPart > 0)
        {
            temp[j++] = static_cast<char>((intPart % 10) + '0');
            intPart /= 10;
        }

        while (j > 0)
            buf[i++] = temp[--j];

        float rounding { 0.5f };
        for (int p{}; p < 4; p++)
            rounding /= 10.0f;
        fracPart += rounding;

        buf[i++] = '.';
        for (int k{}; k < 4; k++)
        {
            fracPart *= 10.0f;
            int const digit{ static_cast<int>(fracPart) };
            buf[i++] = static_cast<char>(digit + '0');
            fracPart -= static_cast<float>(digit);
        }
        buf[i++] = '\r';
        buf[i++] = '\n';
        DWORD written{};
        WriteConsoleA(hStdout, buf, i, &written, nullptr);
    }

    void pancakes_print_string(char const* str, int const len)
    {
        DWORD written{};
        WriteConsoleA(hStdout, str, len, &written, nullptr);
        WriteConsoleA(hStdout, "\r\n", 2, &written, nullptr);
    }

    int pancakes_input(char* buffer, int bufferLen)
    {
        DWORD charsRead{};
        if (!ReadConsoleA(hStdin, buffer, bufferLen - 1, &charsRead, nullptr))
            return 0;

        buffer[charsRead] = '\0';
        return static_cast<int>(charsRead);
    }
}
