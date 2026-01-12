#define WIN32_LEAN_AND_MEAN
#include <cstdint>
#include <windows.h>

extern "C"
{
    int _fltused{ 1 };

    static HANDLE out() { return GetStdHandle(STD_OUTPUT_HANDLE); }
    static HANDLE in()  { return GetStdHandle(STD_INPUT_HANDLE); }

    void pancakes_init()
    {
        DWORD mode{ 0 };
        if (GetConsoleMode(out(), &mode))
            SetConsoleMode(out(), mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    static bool is_console_out()
    {
        DWORD mode;
        return GetConsoleMode(out(), &mode) != 0;
    }

    static bool is_console_in()
    {
        DWORD mode;
        return GetConsoleMode(in(), &mode) != 0;
    }

    float pancakes_parse_float(char const* str, int const len)
    {
        if (len <= 0 || !str)
            return 0.0f;

        double res{ 0.0 };
        double div{ 1.0 };
        int i{ 0 };
        bool neg{ false };
        bool dot{ false };

        while (i < len && (str[i] == ' ' || str[i] == '\t')) i++;

        if (str[i] == '-')
        {
            neg = true;
            ++i;
        }
        else if (str[i] == '+')
            ++i;

        for (; i < len; ++i)
        {
            char const c{ str[i] };
            if (c == '.')
            {
                if (dot) break;
                dot = true;
                continue;
            }

            if (c < '0' || c > '9')
                break;

            if (!dot)
                res = res * 10.0f + static_cast<float>(c - '0');
            else
            {
                div *= 10.0f;
                res += static_cast<float>(c - '0') / div;
            }
        }

        return static_cast<float>(neg ? -res : res);
    }

    void pancakes_print_string(char const* str, int const len)
    {
        if (len <= 0 || !str)
            return;

        int start{ 0 };
        for (int i{ 0 }; i < len; ++i)
        {
            if (str[i] == '\n')
            {
                DWORD written;
                if (i > start)
                    WriteFile(out(), str + start, static_cast<DWORD>(i - start), &written, nullptr);

                if (is_console_out())
                    WriteFile(out(), "\r\n", 2, &written, nullptr);
                else
                    WriteFile(out(), "\n", 1, &written, nullptr);
                start = i + 1;
            }
        }

        if (len > start)
        {
            DWORD written;
            WriteFile(out(), str + start, static_cast<DWORD>(len - start), &written, nullptr);
        }
    }

    void pancakes_print_float(float val)
    {
        char buf[64]{};
        int i{ 0 };

        if (val < 0)
        {
            buf[i++] = '-';
            val = -val;
        }

        val += 0.000005f;

        int intPart{ static_cast<int>(val) };
        float fracPart{ val - static_cast<float>(intPart) };

        char temp[16]{};
        int j{ 0 };
        if (intPart == 0) temp[j++] = '0';
        while (intPart > 0)
        {
            temp[j++] = static_cast<char>(intPart % 10 + '0');
            intPart /= 10;
        }
        while (j > 0)
            buf[i++] = temp[--j];

        if (fracPart > 0.00001f)
        {
            buf[i++] = '.';
            for (int k{ 0 }; k < 4; ++k)
            {
                fracPart *= 10.0f;
                int const digit{ static_cast<int>(fracPart) };
                buf[i++] = static_cast<char>(digit + '0');
                fracPart -= static_cast<float>(digit);
            }

            while (i > 0 && buf[i - 1] == '0') i--;
            if (i > 0 && buf[i - 1] == '.') i--;
        }

        pancakes_print_string(buf, i);
    }

    void pancakes_print_int(std::int32_t const val)
    {
        if (val == 0)
        {
            pancakes_print_string("0", 1);
            return;
        }
        char buf[12];
        int i{ 11 };
        bool const neg{ val < 0 };
        unsigned int n{ neg ? -static_cast<unsigned int>(val) : val };

        while (n > 0)
        {
            buf[i--] = static_cast<char>(n % 10 + '0');
            n /= 10;
        }
        if (neg) buf[i--] = '-';
        pancakes_print_string(&buf[i + 1], 11 - i);
    }

    void pancakes_print_number(float const val)
    {
        if (val == static_cast<float>(static_cast<int32_t>(val)))
            pancakes_print_int(static_cast<int32_t>(val));
        else
            pancakes_print_float(val);
    }

    int pancakes_input(char* buffer, int const bufferLen)
    {
        DWORD read{ 0 };
        if (is_console_in())
        {
            if (!ReadConsoleA(in(), buffer, static_cast<DWORD>(bufferLen - 1), &read, nullptr))
                return 0;
        }
        else if (!ReadFile(in(), buffer, static_cast<DWORD>(bufferLen - 1), &read, nullptr))
            return 0;

        int len{ static_cast<int>(read) };
        while (len > 0 && (buffer[len - 1] == '\r' || buffer[len - 1] == '\n'))
            --len;

        buffer[len] = '\0';
        return len;
    }

    void pancakes_get_windows_size(int* outWidth, int* outHeight)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (!GetConsoleScreenBufferInfo(out(), &csbi))
        {
            *outWidth = 80;
            *outHeight = 25;
            return;
        }

        *outWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *outHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }

    void pancakes_move_cursor_to(int const col, int const row)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (!GetConsoleScreenBufferInfo(out(), &csbi))
            return;

        int const x{ col + csbi.srWindow.Left };
        int const y{ row + csbi.srWindow.Top };

        COORD pos;
        pos.X = static_cast<SHORT>(x < 0 ? 0 : x >= csbi.dwSize.X ? csbi.dwSize.X - 1 : x);
        pos.Y = static_cast<SHORT>(y < 0 ? 0 : y >= csbi.dwSize.Y ? csbi.dwSize.Y - 1 : y);

        SetConsoleCursorPosition(out(), pos);
    }

    void pancakes_get_cursor_pos(int* col, int* row)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(out(), &csbi))
        {
            *col = csbi.dwCursorPosition.X - csbi.srWindow.Left;
            *row = csbi.dwCursorPosition.Y - csbi.srWindow.Top;
        }
        else
        {
            *col = 0;
            *row = 0;
        }
    }
}