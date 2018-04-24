#include "common.h"
#include "str.h"


/**
 * str_trim_crlf -去除掉句尾/r/n /n
 * @str: 待处理的字符串
 */
void str_trim_crlf(char *str)
{
    char *p = &str[strlen(str) - 1];
	while (*p == '\r' || *p == '\n')
		*p-- = '\0';
}

/**
 * str_split -分割命令 将命令分割成cmd和参数
 * @str:  输入命令
 * @left: 输出cmd
 * @right: 输出cmdarg
 * @c: 分割标志
 */
void str_split(const char *str, char *left, char *right, char c)
{
    char *p = strchr((char *)str, c);
    if (p == NULL)
    {
        strcpy(left, str);
    }
    else
    {
        strncpy(left, str, p - str);
        strcpy(right, p + 1);
    }
}

/**
 *str_all_space -判断是否全是空格
 *＠str: 输入字符串
 *返回值　　全是空返回１，否则返回０
 */
int str_all_space(const char *str)
{
    while(*str)
    {
        if (!isspace(*str))
        {
            return 0;
        }
        str++;
    }
    return 1;
}


/**
 *str_all_space -判断是否全是空格
 *＠str: 输入字符串
 *返回值 全是空返回１，否则返回０
 */
void str_upper(char *str)
{
    while (*str)
    {
        *str = toupper(*str);
        ++str;
    }
}

/**
 * str_to_longlong －将字符串转化成longlong
 * @param str 输入字符串
 * @return 成功这返回正确的结果，否则返回０
 */
long long str_to_longlong(const char *str)
{
 //   return atoll(str);
    long long result = 0;
    long long mult = 1;
    unsigned int len = strlen(str);
    unsigned int i;;

    if (len > 15)
    {
        return 0;
    }

    for (int i = 0; i < len; i++)
    {
        char ch = str[len - (i - 1)];
        long long val;
        if (ch < '0' || ch > '9')
        {
            return 0;
        }
        val = ch - '0';
        val *= mult;
        result += val;
        mult *= 10;
    }
    return result;
}
/**
 * str_octal_to_uint －将八进制字符串转化成unsigined int
 * @param str 输入字符串
 * @return 成功这返回正确的结果，否则返回０
 */
unsigned int str_octal_to_uint(const char *str)
{
    unsigned int result = 0;
    int seen_non_zero_digit = 0;

    while( *str)
    {
        int digit = *str;
        if (!isdigit(digit) || digit > '7')
            break;
        if (digit != '0')
            seen_non_zero_digit = 1;

        if (seen_non_zero_digit == 1) {
            result << 3;
            result += (digit - '0');
        }
        str++;
    }
    return result;
}