#define WIN32_LEAN_AND_MEAN
#include <cstddef>
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

    float pancakes_parse_float(char const* str, int len)
    {
        if (len <= 0) return 0.0f;
        float res{};
        float div = 1.0f;
        bool neg = false;
        bool dot = false;
        int i = 0;

        if (str[0] == '-')
        {
            neg = true;
            i = 1;
        }
        else if (str[0] == '+') i = 1;

        if (i >= len || (str[i] < '0' || str[i] > '9') && str[i] != '.')
            return 0.0f;

        for (; i < len; i++)
        {
            if (str[i] == '.')
            {
                if (dot) break;
                dot = true;
                continue;
            }

            if (str[i] < '0' || str[i] > '9') break;

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

    void pancakes_print_float(float val, bool addNewline = true)
    {
        char buf[64];
        char temp[32];
        int i{};

        if (val < 0)
        {
            buf[i++] = '-';
            val = -val;
        }

        int intPart = static_cast<int>(val);
        float fracPart = val - static_cast<float>(intPart);
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

        float rounding = 0.5f;
        for (int p{}; p < 4; p++) rounding /= 10.0f;
        fracPart += rounding;

        buf[i++] = '.';
        for (int k{}; k < 4; k++)
        {
            fracPart *= 10.0f;
            int digit = static_cast<int>(fracPart);
            buf[i++] = static_cast<char>(digit + '0');
            fracPart -= static_cast<float>(digit);
        }

        if (addNewline)
        {
            buf[i++] = '\r';
            buf[i++] = '\n';
        }

        DWORD written{};
        WriteConsoleA(hStdout, buf, i, &written, nullptr);
    }

    void pancakes_print_string(char const* str, int len)
    {
        if (len <= 0 || str == nullptr) return;

        HANDLE hOut{ hStdout != nullptr ? hStdout : GetStdHandle(STD_OUTPUT_HANDLE) };
        int start = 0;
        for (int i = 0; i < len; ++i)
        {
            if (char const c{ str[i] }; c == '\r' || c == '\n')
            {
                if (i > start)
                {
                    DWORD written{};
                    WriteConsoleA(hOut, str + start, i - start, &written, nullptr);
                }

                DWORD written{};
                constexpr char newline[2]{ '\r', '\n' };
                WriteConsoleA(hOut, newline, 2, &written, nullptr);

                if (c == '\r' && i + 1 < len && str[i + 1] == '\n')
                    ++i;

                start = i + 1;
            }
        }

        if (len > start)
        {
            DWORD written{};
            WriteConsoleA(hOut, str + start, len - start, &written, nullptr);
        }
    }

    int pancakes_input(char* buffer, int const bufferLen)
    {
        DWORD charsRead{};
        if (!ReadConsoleA(hStdin, buffer, bufferLen - 1, &charsRead, nullptr))
            return 0;

        buffer[charsRead] = '\0';
        return static_cast<int>(charsRead);
    }

    struct WindowsSize
    {
        int width;
        int height;
    };

    WindowsSize pancakes_get_windows_size()
    {
        if (hStdout == nullptr || hStdin == nullptr)
            pancakes_init();

        HANDLE hOut{ hStdout != nullptr ? hStdout : GetStdHandle(STD_OUTPUT_HANDLE) };
        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        WindowsSize size{ 0, 0 };

        if (GetConsoleScreenBufferInfo(hOut, &csbi))
        {
            size.width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            size.height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }

        DWORD mode{ 0 };
        if (GetConsoleMode(hOut, &mode))
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

        return size;
    }

    int clamp(int const val, int const minVal, int const maxVal)
    {
        if (val < minVal) return minVal;
        if (val > maxVal) return maxVal;
        return val;
    }

    void pancakes_move_cursor_to(int const col, int const row)
    {
        HANDLE hOut{ hStdout != nullptr ? hStdout : GetStdHandle(STD_OUTPUT_HANDLE) };
        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        if (!GetConsoleScreenBufferInfo(hOut, &csbi))
        {
            COORD pos{};
            pos.X = col < 0 ? 0 : static_cast<SHORT>(col);
            pos.Y = row < 0 ? 0 : static_cast<SHORT>(row);
            SetConsoleCursorPosition(hOut, pos);
            return;
        }

        int absX = col + csbi.srWindow.Left;
        int absY = row + csbi.srWindow.Top;

        if (absX < 0) absX = 0;
        if (absY < 0) absY = 0;
        if (absX >= csbi.dwSize.X) absX = csbi.dwSize.X - 1;
        if (absY >= csbi.dwSize.Y) absY = csbi.dwSize.Y - 1;

        COORD pos{};
        pos.X = static_cast<SHORT>(absX);
        pos.Y = static_cast<SHORT>(absY);
        SetConsoleCursorPosition(hOut, pos);
    }

    void pancakes_get_cursor_pos(int* col, int* row)
    {
        HANDLE hOut{ hStdout != nullptr ? hStdout : GetStdHandle(STD_OUTPUT_HANDLE) };
        CONSOLE_SCREEN_BUFFER_INFO csbi{};

        if (GetConsoleScreenBufferInfo(hOut, &csbi))
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
